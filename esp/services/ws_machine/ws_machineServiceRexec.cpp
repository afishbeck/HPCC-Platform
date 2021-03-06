/*##############################################################################

    Copyright (C) 2011 HPCC Systems.

    All rights reserved. This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU Affero General Public License as
    published by the Free Software Foundation, either version 3 of the
    License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU Affero General Public License for more details.

    You should have received a copy of the GNU Affero General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
############################################################################## */

#pragma warning (disable : 4786)

#include "ws_machineService.hpp"
#include "jarray.hpp"
#include "jmisc.hpp"
#include "exception_util.hpp"

//---------------------------------------------------------------------------------------------
//NOTE: PART III of implementation for Cws_machineEx
//      PART I and II are in ws_machineService.cpp and ws_machineServiceMetrics.cpp resp.
//---------------------------------------------------------------------------------------------

static const char* EXEC_FEATURE_URL = "ExecuteAccess";

//---------------------------------------------------------------------------------------------

class CRemoteExecThreadParam : public CWsMachineThreadParam
{
public:
   IMPLEMENT_IINTERFACE;

   Owned<IEspRemoteExecResult> m_pExecResult;
   StringBuffer m_sCommand;
   StringBuffer m_userId;
   StringBuffer m_password;
    StringBuffer m_sConfigAddress;
   bool m_bWait;        
   IEspContext& m_context;

    CRemoteExecThreadParam( const char* pszAddress, const char* cmd, bool wait,
                           Cws_machineEx* pService, IEspContext &context)
                                 : CWsMachineThreadParam(pszAddress, "", pService),
                                   m_sCommand(cmd), m_bWait(wait), m_context(context)
   {
   }

    CRemoteExecThreadParam( const char* pszAddress, const char* pszConfigAddress, const char* cmd, bool wait, 
                                    Cws_machineEx* pService, IEspContext &context)
                                 : CWsMachineThreadParam(pszAddress, "", "", pService),
                                   m_sCommand(cmd), m_bWait(wait), m_context(context)
   {
        m_sConfigAddress = pszConfigAddress;
   }
    virtual void setResultObject(void* pExecResult)
    {
        m_pExecResult.set( static_cast<IEspRemoteExecResult*>( pExecResult ));
    }

    virtual void setResponse( const char* resp )
    {
        m_pExecResult->setResponse( resp );
    }

    virtual void setResultCode( int code )
    {
        m_pExecResult->setResultCode( code );
    }

    virtual void setUserID( const char* userID )
    {
        m_userId.clear().append( userID );
    }

    virtual void setPassword( const char* password )
    {
        m_password.clear().append( password );
    }

   virtual void doWork()
   {
      try
      {
         StringBuffer cmdLine;
         StringBuffer userId;
         StringBuffer password;
         bool bLinux;
         int exitCode = -1;

            if (m_sConfigAddress.length() < 1)
            {
                m_pService->getAccountAndPlatformInfo(m_sAddress.str(), userId, password, bLinux);
            }
            else
            {
                m_pService->getAccountAndPlatformInfo(m_sConfigAddress.str(), userId, password, bLinux);
            }

            if (m_userId.length() < 1 || m_userId.length() < 1)
            {
                //BUG: 9825 - remote execution on linux needs to use individual accounts
                //use userid/password in ESP context for remote execution...
                if (bLinux)
                {
                    userId.clear();
                    password.clear();
                    m_context.getUserID(userId);
                    m_context.getPassword(password);
                }
            }
            else
            {
                userId.clear().append(m_userId);
                password.clear().append(m_password);
            }

#ifdef _WIN32
///#define CHECK_LINUX_COMMAND
#ifndef CHECK_LINUX_COMMAND
#define popen  _popen
#define pclose _pclose

         // Use psexec as default remote control program
         if (bLinux)
         {         
            if (!checkFileExists(".\\plink.exe"))
               throw MakeStringException(ECLWATCH_PLINK_NOT_INSTALLED, "Invalid ESP installation: missing plink.exe to execute the remote program!");

            m_sCommand.replace('\\', '/');//replace all '\\' by '/'

            /* 
            note that if we use plink (cmd line ssh client) for the first time with a computer,
            it generates the following message:

            The server's host key is not cached in the registry. You have no guarantee that the 
            server is the computer you think it is.  The server's key fingerprint is:
            1024 aa:bb:cc:dd:ee:ff:gg:hh:ii:jj:kk:ll:mm:nn:oo:pp
            If you trust this host, enter "y" to add the key to
            PuTTY's cache and carry on connecting.  If you want to carry on connecting just once, 
            without adding the key to the cache, enter "n".If you do not trust this host, press 
            Return to abandon the connection.

            To get around this, we pipe "n" to plink without using its -batch parameter.  We need
            help from cmd.exe to do this though...
            */
            cmdLine.appendf("cmd /c \"echo y | .\\plink.exe -ssh -l %s -pw %s %s sudo bash -c '%s' 2>&1\"",
                environmentConfData.m_user.str(), password.str(), m_sAddress.str(), m_sCommand.str());
         }
         else
         {
            if (!checkFileExists(".\\psexec.exe"))
               throw MakeStringException(ECLWATCH_PSEXEC_NOT_INSTALLED, "Invalid ESP installation: missing psexec.exe to execute the remote program!");

            cmdLine.appendf(".\\psexec \\\\%s -u %s -p %s %s cmd /c %s 2>&1", 
               m_sAddress.str(), userId.str(), password.str(), 
               m_bWait ? "" : "-d", m_sCommand.str());
         }
#else
         if (bLinux)
         {
            m_sCommand.replace('\\', '/');//replace all '\\' by '/'
            cmdLine.appendf("ssh -o StrictHostKeyChecking=no %s '%s'", m_sAddress.str(), m_sCommand.str());
         }
         else
         {
            setResponse("Remote execution from Linux to Windows is not supported!");
            exitCode = 1;
         }
#endif
#else
         if (bLinux)
         {
            m_sCommand.replace('\\', '/');//replace all '\\' by '/'
            cmdLine.appendf("ssh -o StrictHostKeyChecking=no %s '%s'", m_sAddress.str(), m_sCommand.str());
         }
         else
         {
            setResponse("Remote execution from Linux to Windows is not supported!");
            exitCode = 1;
         }
#endif

         if (*cmdLine.str())
         {
            if (m_bWait)
            {
               StringBuffer response, response1;
               exitCode = invoke_program(cmdLine, response);
                    if (exitCode < 0)
                        response1.append("Failed in executing a system command.\n");
                    else
                        response1.append("System command(s) has been executed.\n");

               //remove \n at the end
               int len = response.length();
               if (len > 0 && response.charAt(--len) == '\n')
                  response.setLength(len);

               setResponse(response1.str());
            }
            else
            {
               DWORD runCode;
               ::invoke_program(cmdLine, runCode, false);
               exitCode = (int) runCode;
            }
         }

         setResultCode(exitCode);
      }
      catch(IException* e)
      {
         StringBuffer buf;
         e->errorMessage(buf);
         setResponse(buf.str());
         setResultCode(e->errorCode());
      }
#ifndef NO_CATCHALL
      catch(...)
      {
         setResponse("An unknown exception occurred!");
         setResultCode(-1);
      }
#endif
   }//doWork()

   //---------------------------------------------------------------------------
   //  createProcess
   //---------------------------------------------------------------------------
   int invoke_program(const char *command_line, StringBuffer& response)
   {
      char   buffer[128];
      FILE   *fp;

      /* Run the command so that it writes its output to a pipe. Open this
       * pipe with read text attribute so that we can read it 
       * like a text file. 
       */
        if (getEspLogLevel()>8)
        {
            DBGLOG("command_line=<%s>", command_line);
        }
#ifdef CHECK_LINUX_COMMAND
        return -1;
#else
      if( (fp = popen( command_line, "r" )) == NULL )
         return -1;

        /* Read pipe until end of file. End of file indicates that 
       * the stream closed its standard out (probably meaning it 
       * terminated).
       */
      while ( !feof(fp) )
         if ( fgets( buffer, 128, fp) )
            response.append( buffer );

      /* Close pipe and print return value of CHKDSK. */
      return pclose( fp );
#endif
   }
};

//-------------------------------------------------StartStop--------------------------------------------------

class CStartStopThreadParam : public CRemoteExecThreadParam
{
public:
   IMPLEMENT_IINTERFACE;

   Owned<IEspStartStopResult> m_pResult;
    bool m_bStop;
    bool m_useHPCCInit;

#ifndef OLD_START_STOP
   CStartStopThreadParam( const char* pszAddress, const char* pszConfigAddress, bool bStop, bool useHPCCInit, Cws_machineEx* pService, IEspContext &context)
                        : CRemoteExecThreadParam(pszAddress, pszConfigAddress, "", true, pService, context),
                                  m_bStop(bStop), m_useHPCCInit(useHPCCInit)
   {
   }
#else
    CStartStopThreadParam( const char* pszAddress, bool bStop, bool useHPCCInit, Cws_machineEx* pService, IEspContext &context)
                        : CRemoteExecThreadParam(pszAddress, "", true, pService, context),
                                  m_bStop(bStop), m_useHPCCInit(useHPCCInit)
   {
   }
#endif

    virtual void setResultObject(void* pResult)
    {
        m_pResult.set( static_cast<IEspStartStopResult*>( pResult ));
    }

    virtual void setResponse( const char* resp )
    {
        m_pResult->setResponse( resp );
    }

    virtual void setResultCode( int code )
    {
        m_pResult->setResultCode( code );
    }

   virtual void doWork()
   {
        if (m_useHPCCInit)
        {
            //address specified can be either IP or name of component
            const char* address = m_sAddress.str();
            const char* configAddress = m_sConfigAddress.str();
            if (!address || !*address)
                throw MakeStringException(ECLWATCH_INVALID_IP_OR_COMPONENT, "Invalid address or component name was specified!");

            if (!strchr(address, '.')) //not an IP address
            {
                const char* compType = m_pResult->getCompType();
                const char* compName = address;

                if (!compType || !*compType)
                    throw MakeStringException(ECLWATCH_INVALID_COMPONENT_TYPE, "No component type specified!");

                StringBuffer xpath;
                if (!strcmp(compType, "RoxieCluster"))
                {
                    xpath.append("RoxieServer");
                }
                else if (!strcmp(compType, "ThorCluster"))
                {
                    xpath.append("ThorMaster");
                }
                else if (!strcmp(compType, "HoleCluster"))
                {
                    xpath.append("HoleControl");
                }
                else
                    throw MakeStringException(ECLWATCH_INVALID_COMPONENT_TYPE, "Failed to resolve component type '%s'", compType);

                Owned<IPropertyTree> pComponent = m_pService->getComponent(compType, compName);
                xpath.append("Process[1]/@computer");
                const char* computer = pComponent->queryProp(xpath.str());

                if (!computer || !*computer)
                    throw MakeStringException(ECLWATCH_INVALID_COMPONENT_INFO, "Failed to resolve computer for %s '%s'!", compType, compName);

                Owned<IConstEnvironment> pConstEnv = m_pService->getConstEnvironment();
                Owned<IConstMachineInfo> pConstMc  = pConstEnv->getMachine(computer);

                SCMStringBuffer sAddress;
                pConstMc->getNetAddress(sAddress);

                if (!stricmp(m_sAddress.str(), m_sConfigAddress.str()))
                {
                    m_sAddress.clear().append(sAddress.str());
                    m_sConfigAddress = m_sAddress;
                    m_pResult->setAddress( sAddress.str() );
                }
                else
                {
                    m_sAddress.clear().append(sAddress.str());
                    m_pResult->setAddress( sAddress.str() );
                    if (configAddress && !strchr(configAddress, '.')) //not an IP address
                    {
                        Owned<IPropertyTree> pComponent = m_pService->getComponent(compType, configAddress);
                        xpath.append("Process[1]/@computer");
                        const char* computer = pComponent->queryProp(xpath.str());

                        if (!computer || !*computer)
                            throw MakeStringException(ECLWATCH_INVALID_COMPONENT_INFO, "Failed to resolve computer for %s '%s'!", compType, configAddress);

                        Owned<IConstEnvironment> pConstEnv = m_pService->getConstEnvironment();
                        Owned<IConstMachineInfo> pConstMc  = pConstEnv->getMachine(computer);

                        SCMStringBuffer sAddress;
                        pConstMc->getNetAddress(sAddress);
                        m_sConfigAddress.clear().append(sAddress.str());
                    }
                }
            }

            if ((m_sAddress.length() > 0) && !stricmp(m_sAddress.str(), "."))
            {
                StringBuffer ipStr;
                IpAddress ipaddr = queryHostIP();
                ipaddr.getIpText(ipStr);
                if (ipStr.length() > 0)
                {
#ifdef MACHINE_IP
                    m_sAddress.clear().append(MACHINE_IP);
#else
                    m_sAddress.clear().append(ipStr.str());
#endif
                    m_pResult->setAddress( m_sAddress.str() );
                }
            }

#ifdef OLD_START_STOP
            int OS = m_pResult->getOS();

            StringBuffer sPath( m_pResult->getPath() );
            if (OS == 0)
                sPath.replace('$', ':');
            else
                if (sPath.charAt(0) != '/')
                    sPath.insert(0, '/');
            m_sCommand.clear().append(sPath).append( OS==0 ? '\\' : '/');
            m_sCommand.append(m_bStop ? "stop" : "startup");
            if (OS == 0)
                m_sCommand.append(".bat");
            m_sCommand.append(' ');

            m_sCommand.append(sPath);
#else
            StringBuffer sPath( m_pResult->getPath() );
            if (sPath.charAt(sPath.length() - 1) == '/')
                sPath.setLength(sPath.length() - 1);
            if (sPath.length() > 0)
            {
                char* pStr = (char*) sPath.str();
                char* ppStr = strchr(pStr, '/');
                while (ppStr)
                {
                    ppStr++;
                    pStr = ppStr;
                    ppStr = strchr(pStr, '/');
                }

                if (!m_bStop)
                    m_sCommand.appendf("sudo /etc/init.d/hpcc-init -c %s start", pStr);
                else
                    m_sCommand.appendf("sudo /etc/init.d/hpcc-init -c %s stop", pStr);
            }
#endif
            m_pResult->setCommand( m_sCommand.str() );
        }
        else
        {
            //address specified can be either IP or name of component
            const char* address = m_sAddress.str();
            if (!address || !*address)
                throw MakeStringException(ECLWATCH_INVALID_IP_OR_COMPONENT, "Invalid address or component name was specified!");

            if (!strchr(address, '.')) //not an IP address
            {
                const char* compType = m_pResult->getCompType();
                const char* compName = address;

                if (!compType || !*compType)
                    throw MakeStringException(ECLWATCH_INVALID_COMPONENT_TYPE, "No component type specified!");

                StringBuffer xpath;
                if (!strcmp(compType, "RoxieCluster"))
                {
                    xpath.append("RoxieServer");
                }
                else if (!strcmp(compType, "ThorCluster"))
                {
                    xpath.append("ThorMaster");
                }
                else if (!strcmp(compType, "HoleCluster"))
                {
                    xpath.append("HoleControl");
                }
                else
                    throw MakeStringException(ECLWATCH_INVALID_COMPONENT_TYPE, "Failed to resolve component type '%s'", compType);

                Owned<IPropertyTree> pComponent = m_pService->getComponent(compType, compName);
                xpath.append("Process[1]/@computer");
                const char* computer = pComponent->queryProp(xpath.str());

                if (!computer || !*computer)
                    throw MakeStringException(ECLWATCH_INVALID_COMPONENT_INFO, "Failed to resolve computer for %s '%s'!", compType, compName);

                Owned<IConstEnvironment> pConstEnv = m_pService->getConstEnvironment();
                Owned<IConstMachineInfo> pConstMc  = pConstEnv->getMachine(computer);

                SCMStringBuffer sAddress;
                pConstMc->getNetAddress(sAddress);

                m_sAddress.clear().append(sAddress.str());
                m_pResult->setAddress( sAddress.str() );
            }

            int OS = m_pResult->getOS();

            StringBuffer sPath( m_pResult->getPath() );
            if (OS == 0)
                sPath.replace('$', ':');
            else
                if (sPath.charAt(0) != '/')
                    sPath.insert(0, '/');

            m_sCommand.clear().append(sPath).append( OS==0 ? '\\' : '/');
            m_sCommand.append(m_bStop ? "stop" : "startup");
            if (OS == 0)
                m_sCommand.append(".bat");
            m_sCommand.append(' ');

            m_sCommand.append(sPath);
            m_pResult->setCommand( m_sCommand.str() );
        }
        CRemoteExecThreadParam::doWork();
    }
};

void Cws_machineEx::ConvertAddress( const char* originalAddress, StringBuffer& newAddress)
{
    if (!originalAddress || !*originalAddress)
        throw MakeStringException(ECLWATCH_INVALID_IP_OR_COMPONENT, "No network address or computer name specified!");

    StringArray sArray;
    DelimToStringArray(originalAddress, sArray, ":");

    if (sArray.ordinality() < 4)
        throw MakeStringException(ECLWATCH_MISSING_PARAMS, "Incomplete arguments");

    const char* address = sArray.item(0);
    const char* compType= sArray.item(1);
    const char* compName= sArray.item(2);
    const char* OS        = sArray.item(3);
    const char* path      = sArray.item(4);

    StringBuffer process;
    if (sArray.ordinality() > 5)
    {
        const char* ClusterType   = sArray.item(5);
        if (ClusterType && *ClusterType)
        {
            if (strcmp("THORMACHINES",ClusterType) == 0)
            {
                process.append("ThorMasterProcess");
            }
            else if (strcmp("ROXIEMACHINES",ClusterType) == 0)
            {
                process.append("RoxieServerProcess");
            }
        }
    }

    if (strchr(address, '.')) //have an IP address
    {
        newAddress.clear().append(originalAddress);
        return;
    }

    StringBuffer xpath;
    if (!strcmp(compType, "RoxieCluster"))
    {
        xpath.append("RoxieServer");
    }
    else if (!strcmp(compType, "ThorCluster"))
    {
        xpath.append("ThorMaster");
    }
    else if (!strcmp(compType, "HoleCluster"))
    {
        xpath.append("HoleControl");
    }
    else
        throw MakeStringException(ECLWATCH_INVALID_COMPONENT_TYPE, "Failed to resolve address for component type '%s'", compType);

    Owned<IPropertyTree> pComponent = getComponent(compType, address);
    xpath.append("Process[1]/@computer");
    const char* computer = pComponent->queryProp(xpath.str());

    if (!computer || !*computer)
        throw MakeStringException(ECLWATCH_INVALID_COMPONENT_INFO, "Failed to resolve computer for %s '%s'!", compType, address);

    Owned<IConstEnvironment> pConstEnv = getConstEnvironment();
    Owned<IConstMachineInfo> pConstMc  = pConstEnv->getMachine(computer);

    SCMStringBuffer sAddress;
    pConstMc->getNetAddress(sAddress);
#ifndef OLD_START_STOP
    {
        StringBuffer sConfigAddress;
        sConfigAddress.append(sAddress.str());

        if (!strcmp(sAddress.str(), "."))
        {
            StringBuffer ipStr;
            IpAddress ipaddr = queryHostIP();
            ipaddr.getIpText(ipStr);
            if (ipStr.length() > 0)
            {
#ifdef MACHINE_IP
                sAddress.set(MACHINE_IP);
#else
                sAddress.set(ipStr.str());
#endif
            }
        }

        if (process.length() > 0)
            newAddress.clear().appendf("%s|%s:%s:%s:%s:%s", sAddress.str(), sConfigAddress.str(), process.str(), compName, OS, path);
        else
            newAddress.clear().appendf("%s|%s:%s:%s:%s:%s", sAddress.str(), sConfigAddress.str(), compType, compName, OS, path);
    }
#else
        if (process.length() > 0)
            newAddress.clear().appendf("%s:%s:%s:%s:%s", sAddress.str(), process.str(), compName, OS, path);
        else
            newAddress.clear().appendf("%s:%s:%s:%s:%s", sAddress.str(), compType, compName, OS, path);
        //newAddress.clear().appendf("%s:ThorMasterProcess:%s:%s:%s", sAddress.str(), compName, OS, path);
#endif
    return;
}

bool Cws_machineEx::doStartStop(IEspContext &context, StringArray& addresses, char* userName, char* password, bool bStop,
                                                     IEspStartStopResponse &resp)
{
    bool containCluster = false;
    double version = context.getClientVersion();
    const int ordinality= addresses.ordinality();

    UnsignedArray threadHandles;
    IArrayOf<IEspStartStopResult> resultsArray;

    for (int index=0; index<ordinality; index++)
    {
        const char* address0 = addresses.item(index);

        //address passed in is of the form "192.168.1.4:EspProcess:2:path1"
        StringArray sArray;
        DelimToStringArray(addresses.item(index), sArray, ":");

        if (sArray.ordinality() < 4)
            throw MakeStringException(ECLWATCH_MISSING_PARAMS, "Incomplete arguments");

        Owned<IEspStartStopResult> pResult = static_cast<IEspStartStopResult*>(new CStartStopResult(""));
        const char* address = sArray.item(0);
        const char* compType= sArray.item(1);
        const char* OS        = sArray.item(3);//index 2 is component name
        const char* path      = sArray.item(4);

        if (!(address && *address && compType && *compType && OS && *OS && path && *path))
            throw MakeStringExceptionDirect(ECLWATCH_INVALID_INPUT, "Invalid input");

        if (!stricmp(compType, "ThorCluster") || !stricmp(compType, "RoxieCluster"))
            containCluster = true;

#ifndef OLD_START_STOP
        {
            char* configAddress = NULL;
            char* props1 = (char*) strchr(address, '|');
            if (props1)
            {
                configAddress = props1+1;
                *props1 = '\0';
            }
            else
            {
                configAddress = (char*) address;
            }

            StringBuffer newAddress;
            ConvertAddress(address0, newAddress);
            pResult->setAddressOrig ( newAddress.str() );//can be either IP or name of component
            pResult->setAddress ( address );//can be either IP or name of component
            pResult->setCompType( compType );
            if (version > 1.04)
            {       
                pResult->setName( path );
                const char* pStr2 = strstr(path, "LexisNexis");
                if (pStr2)
                {
                    char name[256];
                    const char* pStr1 = strchr(pStr2, '|');
                    if (!pStr1)
                    {
                        strcpy(name, pStr2+11);
                    }
                    else
                    {
                        strncpy(name, pStr2+11, pStr1 - pStr2 -11);
                        name[pStr1 - pStr2 -11] = 0;
                    }
                    pResult->setName( name );
                }   
            }
            
            pResult->setOS( atoi(OS) ); 
            pResult->setPath( path );

            resultsArray.append(*pResult.getLink());

            CStartStopThreadParam* pThreadReq;
            pThreadReq = new CStartStopThreadParam(address, configAddress, bStop, m_useDefaultHPCCInit, this, context);
            pThreadReq->setResultObject( pResult );

            if (userName && *userName)
                pThreadReq->setUserID( userName );
            if (password && *password)
                pThreadReq->setPassword( password );

            PooledThreadHandle handle = m_threadPool->start( pThreadReq );
            threadHandles.append(handle);
        }
#else
        {
            StringBuffer newAddress;
            ConvertAddress(address0, newAddress);
            char* pStr = (char*) strchr(address, '|');;
            if (pStr)
                pStr[0] = 0;

            pResult->setAddressOrig ( newAddress.str() );//can be either IP or name of component
            pResult->setAddress ( address );//can be either IP or name of component
            pResult->setCompType( compType );
            pResult->setOS( atoi(OS) ); 
            pResult->setPath( path );

            resultsArray.append(*pResult.getLink());

            CStartStopThreadParam* pThreadReq;
            pThreadReq = new CStartStopThreadParam(address, bStop, this, context);
            pThreadReq->setResultObject( pResult );

            if (userName && *userName)
                pThreadReq->setUserID( userName );
            if (password && *password)
                pThreadReq->setPassword( password );

            PooledThreadHandle handle = m_threadPool->start( pThreadReq );
            threadHandles.append(handle);
        }
#endif
    }

    //block for worker theads to finish, if necessary, and then collect results
    //
    PooledThreadHandle* pThreadHandle = threadHandles.getArray();
    unsigned i=threadHandles.ordinality();
    while (i--) 
    {
        m_threadPool->join(*pThreadHandle, 30000);//abort after 30 secs in remote possibility that the command blocks
        pThreadHandle++;
    }

    resp.setStartStopResults(resultsArray);
    resp.setStop(bStop);

    if (version > 1.08)
    {
        resp.setContainCluster(containCluster);
    }
    return true;
}

bool Cws_machineEx::onStartStop( IEspContext &context, IEspStartStopRequest &req, 
                                         IEspStartStopResponse &resp)
{
    try
    {
        if (!context.validateFeatureAccess(EXEC_FEATURE_URL, SecAccess_Full, false))
            throw MakeStringException(ECLWATCH_EXECUTION_ACCESS_DENIED, "Permission denied.");

        char* userName = (char*) m_sTestStr1.str();
        char* password = (char*) m_sTestStr2.str();
        doStartStop(context, req.getAddresses(), userName, password, req.getStop(), resp);
    }
    catch(IException* e)
    {   
        FORWARDEXCEPTION(context, e,  ECLWATCH_INTERNAL_ERROR);
    }
    return true;
}

void Cws_machineEx::updatePathInAddress(const char* address, StringBuffer& addrStr)
{
    addrStr.append(address);

    StringArray sArray;
    DelimToStringArray(address, sArray, ":");
    const char* OS    = sArray.item(3);
    const char* Dir  = sArray.item(4);
    if (OS && *OS && Dir && *Dir)
    {
        char oldC1 = '/';
        char oldC2 = '$';
        char newC1 = '\\';
        char newC2 = ':';
        int os = atoi(OS);      
        if (os == 2) //2: linux
        {
            oldC1 = '\\';
            oldC2 = ':';
            newC1 = '/';
            newC2 = '$';
        }
        StringBuffer dirStr(Dir);
        dirStr.replace(oldC1, newC1);
        dirStr.replace(oldC2, newC2);
        if ((os == 2) && (dirStr.charAt(0) != '/'))
            dirStr.insert(0, '/');

        addrStr.clear();
        for (unsigned i = 0; i < sArray.length(); i++)
        {
            const char* item  = sArray.item(i);
            if (i == 4)
                addrStr.appendf(":%s", dirStr.str());
            else if (item && *item)
            {
                if (i == 0)
                    addrStr.append(item);
                else
                    addrStr.appendf(":%s", item);
            }
        }
    }

    return;
}

bool Cws_machineEx::onStartStopBegin( IEspContext &context, IEspStartStopBeginRequest &req, 
                                         IEspStartStopBeginResponse &resp)
{
    try
    {
        if (!context.validateFeatureAccess(EXEC_FEATURE_URL, SecAccess_Full, false))
            throw MakeStringException(ECLWATCH_EXECUTION_ACCESS_DENIED, "Permission denied.");

        StringBuffer addresses;
        StringArray& addresses0 = req.getAddresses();
        for(unsigned i = 0; i < addresses0.length(); i++)
        {
            StringBuffer addrStr;
            const char* address = addresses0.item(i);
            updatePathInAddress(address, addrStr);
            if (i > 0)
                addresses.appendf("|Addresses_i%d=%s", i+1, addrStr.str());
            else
                addresses.appendf("Addresses_i1=%s", addrStr.str());
        }

        resp.setAddresses(addresses);
        resp.setKey1(req.getKey1());
        resp.setKey2(req.getKey2());
        resp.setStop(req.getStop());
        double version = context.getClientVersion();
        if (version > 1.07)
        {
            resp.setAutoRefresh( req.getAutoRefresh() );
            resp.setMemThreshold(req.getMemThreshold());
            resp.setDiskThreshold(req.getDiskThreshold());
            resp.setCpuThreshold(req.getCpuThreshold());
            resp.setMemThresholdType(req.getMemThresholdType());
            resp.setDiskThresholdType(req.getDiskThresholdType());
        }
    }
    catch(IException* e)
    {   
        FORWARDEXCEPTION(context, e,  ECLWATCH_INTERNAL_ERROR);
    }
    return true;
}

bool Cws_machineEx::onStartStopDone( IEspContext &context, IEspStartStopDoneRequest &req, 
                                         IEspStartStopResponse &resp)
{
    try
    {
        if (!context.validateFeatureAccess(EXEC_FEATURE_URL, SecAccess_Full, false))
            throw MakeStringException(ECLWATCH_EXECUTION_ACCESS_DENIED, "Permission denied.");

        const char*addresses0 = req.getAddresses();
        bool bStop = req.getStop();

        char* userName = (char*) m_sTestStr1.str();
        char* password = (char*) m_sTestStr2.str();

        StringArray addresses;
        char* pAddr = (char*) addresses0;
        while (pAddr)
        {
            char* ppAddr = strstr(pAddr, "|Addresses_");
            if (!ppAddr)
            {
                char* ppAddr0 = strchr(pAddr, '=');
                if (!ppAddr0)
                    addresses.append(pAddr);
                else
                    addresses.append(ppAddr0+1);
                break;
            }
            else
            {
                char addr[1024];
                strncpy(addr, pAddr, ppAddr - pAddr);
                addr[ppAddr - pAddr] = 0;
                char* ppAddr0 = strchr(addr, '=');
                if (!ppAddr0)
                    addresses.append(addr);
                else
                    addresses.append(ppAddr0+1);

                pAddr = ppAddr + 1;
            }
        }

        doStartStop(context, addresses, userName, password, bStop, resp);

        double version = context.getClientVersion();
        if (version > 1.07)
        {
            resp.setAutoRefresh( req.getAutoRefresh() );
            resp.setMemThreshold(req.getMemThreshold());
            resp.setDiskThreshold(req.getDiskThreshold());
            resp.setCpuThreshold(req.getCpuThreshold());
            resp.setMemThresholdType(req.getMemThresholdType());
            resp.setDiskThresholdType(req.getDiskThresholdType());
        }
    }
    catch(IException* e)
    {   
        FORWARDEXCEPTION(context, e,  ECLWATCH_INTERNAL_ERROR);
    }
    return true;
}

