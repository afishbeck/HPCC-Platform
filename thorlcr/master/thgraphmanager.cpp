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

#include "build-config.h"
#include "platform.h"
#include "jarray.hpp"
#include "jfile.hpp"
#include "jmutex.hpp"
#include "jlog.hpp"

#include "portlist.h"
#include "wujobq.hpp"
#include "daclient.hpp"

#include "thgraphmaster.ipp"
#include "thorport.hpp"

#include "thmem.hpp"
#include "thmastermain.hpp"
#include "thexception.hpp"
#include "thcodectx.hpp"

#include "daaudit.hpp"
#include "dadfs.hpp"
#include "dafdesc.hpp"
#include "dautils.hpp"
#include "dllserver.hpp"
#include "eclhelper.hpp"
#include "swapnodelib.hpp"
#include "thactivitymaster.ipp"
#include "thdemonserver.hpp"
#include "thgraphmanager.hpp"

class CJobManager : public CSimpleInterface, implements IJobManager, implements IExceptionHandler
{
    bool stopped, handlingConversation;
    Owned<IConversation> conversation;
    StringAttr queueName;
    CriticalSection replyCrit, jobCrit;
    CFifoFileCache querySoCache;
    Owned<IJobQueue> jobq;
    ICopyArrayOf<CJobMaster> jobs;

    Owned<IDeMonServer> demonServer;
    atomic_t            activeTasks;
    StringAttr          currentWuid;
    ILogMsgHandler *logHandler;

    bool executeGraph(IConstWorkUnit &workunit, const char *graphName, const SocketEndpoint &agentEp);
    void addJob(CJobMaster &job) { CriticalBlock b(jobCrit); jobs.append(job); }
    void removeJob(CJobMaster &job) { CriticalBlock b(jobCrit); jobs.zap(job); }

public:
    IMPLEMENT_IINTERFACE_USING(CSimpleInterface);

    CJobManager(ILogMsgHandler *logHandler);
    ~CJobManager();

    bool doit(IConstWorkUnit *workunit, const char *graphName, const SocketEndpoint &agentep);
    void reply(IConstWorkUnit *workunit, const char *wuid, IException *e, const SocketEndpoint &agentep, bool allDone);

    void run();

// IExceptionHandler
    bool fireException(IException *e);

// IJobManager
    virtual void stop();
    virtual void replyException(CJobMaster &job, IException *e);
    virtual void setWuid(const char *wuid, const char *cluster=NULL);
    virtual IDeMonServer *queryDeMonServer() { return demonServer; }
    virtual void fatal(IException *e);
    virtual void addCachedSo(const char *name);
};

// CJobManager impl.

CJobManager::CJobManager(ILogMsgHandler *_logHandler) : logHandler(_logHandler)
{
    stopped = handlingConversation = false;
    addThreadExceptionHandler(this);
    if (globals->getPropBool("@watchdogEnabled"))
        demonServer.setown(createDeMonServer());
    else
        globals->setPropBool("@watchdogProgressEnabled", false);
    atomic_set(&activeTasks, 0);
    setJobManager(this);
}

CJobManager::~CJobManager()
{
    setJobManager(NULL);
    removeThreadExceptionHandler(this);
}

void CJobManager::stop()
{
    if (!stopped)
    {
        LOG(MCdebugProgress, thorJob, "Stopping jobManager");
        stopped = true;
        if (jobq)
        {
            jobq->cancelWaitStatsChange();
            jobq->cancelAcceptConversation();
        }
        if (conversation && !handlingConversation)
            conversation->cancel();
    }
}

void CJobManager::fatal(IException *e)
{
    try
    {
        IArrayOf<CJobMaster> jobList;
        {
            CriticalBlock b(jobCrit);
            ForEachItemIn(j, jobs)
                jobList.append(*LINK(&jobs.item(j)));
        }
        ForEachItemIn(j, jobList)
            replyException(jobList.item(j), e);
        jobList.kill();

        if (globals->getPropBool("@watchdogProgressEnabled"))
            queryDeMonServer()->endGraphs();
        setWuid(NULL); // deactivate workunit status (Shouldn't this logic belong outside of thor?)

    }
    catch (IException *e)
    {
        EXCLOG(e, NULL);
        e->Release();
    }
    catch (...)
    {
        ERRLOG("Unknown exception in CJobManager::fatal");
    }
    LOG(daliAuditLogCat,",Progress,Thor,Terminate,%s,%s,%s,exception",
            queryServerStatus().queryProperties()->queryProp("@thorname"),
            queryServerStatus().queryProperties()->queryProp("@nodeGroup"),
            queryServerStatus().queryProperties()->queryProp("@queue"));
    
    queryLogMsgManager()->flushQueue(10*1000);

#ifdef _WIN32
    TerminateProcess(GetCurrentProcess(), 1);
#else
    kill(getpid(), SIGKILL);
#endif
}


#define IDLE_RESTART_PERIOD (8*60) // 8 hours
class CIdleShutdown : public CSimpleInterface, implements IThreaded
{
    unsigned timeout;
    Semaphore sem;
    CThreaded threaded;
public:
    CIdleShutdown(unsigned _timeout) : timeout(_timeout*60000), threaded("CIdleShutdown") { threaded.init(this); }
    ~CIdleShutdown() { stop(); threaded.join(); }
    virtual void main()
    {
        if (!sem.wait(timeout)) // feeling neglected, restarting..
            abortThor(MakeThorException(TE_IdleRestart, "Thor has been idle for %d minutes, restarting", timeout/60000));
    }
    void stop() { sem.signal(); }
};

static int getRunningMaxPriority(const char *qname)
{
    int maxpriority = 0; // ignore neg
    try {
        Owned<IRemoteConnection> conn = querySDS().connect("/Status/Servers",myProcessSession(),RTM_LOCK_READ,30000);
        if (conn.get()) 
        {
            Owned<IPropertyTreeIterator> it(conn->queryRoot()->getElements("Server"));
            ForEach(*it) {
                StringBuffer instance;
                if(it->query().hasProp("@queue"))
                {
                    const char* queue=it->query().queryProp("@queue");
                    if(queue&&(strcmp(queue,qname)==0)) {
                        Owned<IPropertyTreeIterator> wuids = it->query().getElements("WorkUnit");
                        ForEach(*wuids) {
                            IPropertyTree &wu = wuids->query();
                            const char* wuid=wu.queryProp(NULL);
                            if (wuid&&*wuid) {
                                Owned<IWorkUnitFactory> factory = getWorkUnitFactory();
                                Owned<IConstWorkUnit> workunit = factory->openWorkUnit(wuid, false);
                                if (workunit) {
                                    int priority = workunit->getPriorityValue();
                                    if (priority>maxpriority)
                                        maxpriority = priority;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    catch (IException *e)
    {
        EXCLOG(e,"getRunningMaxPriority");
        e->Release();
    }
    return maxpriority;

}

bool CJobManager::fireException(IException *e)
{
    IArrayOf<CJobMaster> jobList;
    {
        CriticalBlock b(jobCrit);
        ForEachItemIn(j, jobs)
            jobList.append(*LINK(&jobs.item(j)));
    }
    ForEachItemIn(j, jobList)
        jobList.item(j).fireException(e);
    jobList.kill();
    return true;
}

void CJobManager::run()
{
    setWuid(NULL);
    StringBuffer soPath;
    globals->getProp("@query_so_dir", soPath);
    StringBuffer soPattern("*.");
#ifdef _WIN32
    soPattern.append("dll");
#else
    soPattern.append("so");
#endif
    querySoCache.init(soPath.str(), DEFAULT_QUERYSO_LIMIT, soPattern);

    SCMStringBuffer _queueNames;
    const char *thorName = globals->queryProp("@name");
    if (!thorName) thorName = "thor";
    getThorQueueNames(_queueNames, thorName);
    queueName.set(_queueNames.str());

    jobq.setown(createJobQueue(queueName.get()));
    struct cdynprio: public IDynamicPriority
    {
        const char *qn;
        int get() 
        {
            int p = getRunningMaxPriority(qn);
            if (p)
                PROGLOG("Dynamic Min priority = %d",p);
            return p;
        }
    } *dp = NULL;
    
    if (globals->getPropBool("@multiThorPriorityLock")) {
        PROGLOG("multiThorPriorityLock enabled");
        dp = new cdynprio;
        dp->qn = queueName.get();
    }

    class CThorListener : public CSimpleInterface, implements IThreaded
    {
        bool stopped;
        CThreaded threaded;
        mptag_t mptag;
        CJobManager &jobManager;
    public:
        CThorListener(CJobManager &_jobManager, mptag_t _mptag) : threaded("CDaliConnectionValidator"), jobManager(_jobManager), mptag(_mptag)
        {
            stopped = false;
            threaded.init(this);
        }
        ~CThorListener() { stop(); threaded.join(); }
        void stop()
        {
            stopped = true;
            queryWorldCommunicator().cancel(NULL, mptag);
        }
        void main()
        {
            loop
            {
                CMessageBuffer msg;
                if (!queryWorldCommunicator().recv(msg, NULL, mptag))
                    break;
                
                StringAttr cmd;
                msg.read(cmd);
                if (0 == stricmp("stop", cmd))
                {
                    setExitCode(0);
                    bool stopCurrentJob;
                    msg.read(stopCurrentJob);
                    abortThor(NULL, stopCurrentJob);
                    break;
                }
                else
                    PROGLOG("Unknown cmd = %s", cmd.get());
            }
        }
    } stopThorListener(*this, MPTAG_THOR);
    StringBuffer exclusiveLockName;
    Owned<IDaliMutex> exclLockDaliMutex;
    if (globals->getProp("@multiThorExclusionLockName",exclusiveLockName))
    {
        if (exclusiveLockName.length())
        {
            if (globals->getPropBool("@multiThorPriorityLock"))
                FLLOG(MCoperatorWarning, thorJob, "multiThorPriorityLock cannot be used in conjunction with multiThorExclusionLockName");
            else
            {
                PROGLOG("Multi-Thor exclusive lock defined: %s", exclusiveLockName.str());
                exclLockDaliMutex.setown(createDaliMutex(exclusiveLockName.str()));
            }
        }
    }
    bool jobQConnected = false;
    while (!stopped)
    {
        handlingConversation = false;
        conversation.clear();
        SocketEndpoint masterEp(getMasterPortBase());
        StringBuffer url;
        PROGLOG("ThorLCR(%s) available, waiting on queue %s",masterEp.getUrlStr(url).str(),queueName.get());

        struct CLock
        {
            IDaliMutex *lock;
            StringAttr name;
            CLock() : lock(NULL) { }
            ~CLock()
            {
                clear();
            }
            void set(IDaliMutex *_lock, const char *_name)
            {
                lock = _lock;
                name.set(_name);
                PROGLOG("Took exclusive lock %s", name.get());
            }
            void clear()
            {
                if (lock)
                {
                    IDaliMutex *_lock = lock;
                    lock = NULL;
                    _lock->leave();
                    PROGLOG("Cleared exclusive lock: %s", name.get());
                }
            }
        } daliLock;
        Owned<IJobQueueItem> item;
        {
            CIdleShutdown idleshutdown(globals->getPropInt("@idleRestartPeriod", IDLE_RESTART_PERIOD));
            if (exclLockDaliMutex.get())
            {
                loop
                {
                    while (!stopped && !jobq->ordinality()) // this is avoid tight loop when nothing on q.
                    {
                        if (jobQConnected)
                        {
                            jobq->disconnect();
                            jobQConnected = false;
                        }
                        jobq->waitStatsChange(1000);
                    }
                    if (stopped)
                        break;
                    unsigned connected, waiting, enqueued;
                    if (exclLockDaliMutex->enter(5000))
                    {
                        daliLock.set(exclLockDaliMutex, exclusiveLockName);
                        if (jobq->ordinality())
                        {
                            if (!jobQConnected)
                            {
                                jobq->connect();
                                jobQConnected = true;
                            }
                            // NB: this is expecting to get an item without delay, timeout JIC.
                            unsigned t = msTick();
                            Owned<IJobQueueItem> _item = jobq->dequeue(30*1000);
                            unsigned e = msTick() - t;
                            StringBuffer msg;
                            if (_item.get())
                                msg.append("Jobqueue item retreived");
                            else
                                msg.append("Nothing found on jobq::dequeue");
                            if (e>=5000)
                                msg.append(" - acceptConversation took ").append(e/1000).append(" secs");
                            PROGLOG("%s", msg.str());
                            if (_item.get())
                            {
                                if (_item->isValidSession())
                                {
                                    SocketEndpoint ep = _item->queryEndpoint();
                                    ep.port = _item->getPort();
                                    Owned<IConversation> acceptconv = createSingletonSocketConnection(ep.port,&ep);
                                    if (acceptconv->connect(60*1000)) // shouldn't need that long
                                    {
                                        item.setown(_item.getClear());
                                        conversation.setown(acceptconv.getClear());
                                    }
                                    break;
                                }
                            }
                        }
                        daliLock.clear();
                    }
                    else
                    {
                        jobq->getStats(connected, waiting, enqueued);
                        if (enqueued)
                            PROGLOG("Exclusive lock %s in use. Queue state (connected=%d, waiting=%d, enqueued=%d)", exclusiveLockName.str(), connected ,waiting, enqueued);
                    }
                }
            }
            else
            {
                if (!jobQConnected)
                {
                    jobq->connect();
                    jobQConnected = true;
                }
                IJobQueueItem *_item;
                conversation.setown(jobq->acceptConversation(_item,30*1000,dp));    // 30s priority transition delay
                item.setown(_item);
            }
        }
        if (!conversation.get()||!item.get())
        {
            if (!stopped)
                setExitCode(0); 
            PROGLOG("acceptConversation aborted - terminating");
            break;
        }
        StringAttr graphName;
        handlingConversation = true;
        SocketEndpoint agentep;
        try
        {
            MemoryBuffer msg;
            masterEp.serialize(msg);  // only used for tracing
            if (!conversation->send(msg))
            {
                WARNLOG("send conversation failed");
                continue;
            }
            if (!conversation->recv(msg.clear(),60*1000))
            {
                WARNLOG("recv conversation failed");
                continue;
            }
            msg.read(graphName);
            agentep.deserialize(msg);
        }
        catch (IException *e)
        {
            FLLOG(MCoperatorWarning, thorJob, e, "CJobManager::run");
            continue;
        }
        Owned<IWorkUnitFactory> factory;
        Owned<IConstWorkUnit> workunit;
        const char *wuid = item->queryWUID();
        bool allDone = false;
        try
        {
            factory.setown(getWorkUnitFactory());
            workunit.setown(factory->openWorkUnit(wuid, false));
            if (!workunit) // check workunit is available and ready to run.
                throw MakeStringException(0, "Could not locate workunit %s", wuid);
            if ((workunit->getCodeVersion() > ACTIVITY_INTERFACE_VERSION) || (workunit->getCodeVersion() < MIN_ACTIVITY_INTERFACE_VERSION))
                throw MakeStringException(0, "Workunit was compiled for eclagent interface version %d, this thor requires version %d..%d", workunit->getCodeVersion(), MIN_ACTIVITY_INTERFACE_VERSION, ACTIVITY_INTERFACE_VERSION);
            allDone = doit(workunit, graphName, agentep);
            daliLock.clear();
            reply(workunit, wuid, NULL, agentep, allDone);
        }
        catch (IException *e) 
        { 
            IThorException *te = QUERYINTERFACE(e, IThorException);
            if (te && tea_shutdown==te->queryAction()) 
                stopped = true;
            reply(workunit, wuid, e, agentep, false); 
        }
        catch (CATCHALL) 
        { 
            reply(workunit, wuid, MakeStringException(0, "Unknown exception"), agentep, false); 
        }
    }
    delete dp;
    jobq.clear();
}

bool CJobManager::doit(IConstWorkUnit *workunit, const char *graphName, const SocketEndpoint &agentep)
{
    StringBuffer s;
    SCMStringBuffer _wuid;
    workunit->getWuid(_wuid);
    const char *wuid = _wuid.str();
    LOG(MCdebugInfo, thorJob, "Processing wuid=%s, graph=%s from agent: %s", wuid, graphName, agentep.getUrlStr(s).str());

    SCMStringBuffer user;
    workunit->getUser(user);

    LOG(daliAuditLogCat,",Progress,Thor,Start,%s,%s,%s,%s,%s,%s",
            queryServerStatus().queryProperties()->queryProp("@thorname"),
            wuid,
            graphName,
            user.str(),
            queryServerStatus().queryProperties()->queryProp("@nodeGroup"),
            queryServerStatus().queryProperties()->queryProp("@queue"));
    Owned<IException> e;
    bool allDone = false;
    try
    {
        allDone = executeGraph(*workunit, graphName, agentep);
    }
    catch (IException *_e) { e.setown(_e); }
    LOG(daliAuditLogCat,",Progress,Thor,Stop,%s,%s,%s,%s,%s,%s",
            queryServerStatus().queryProperties()->queryProp("@thorname"),
            wuid,
            graphName,
            user.str(),
            queryServerStatus().queryProperties()->queryProp("@nodeGroup"),
            queryServerStatus().queryProperties()->queryProp("@queue"));
    if (e.get()) throw e.getClear();
    return allDone;
}


void CJobManager::setWuid(const char *wuid, const char *cluster)
{
    currentWuid.set(wuid);
    try
    {
        if (wuid && *wuid)
        {
            queryServerStatus().queryProperties()->setProp("WorkUnit", wuid);
            queryServerStatus().queryProperties()->setProp("Cluster", cluster);
        }
        else
        {
            queryServerStatus().queryProperties()->removeProp("WorkUnit");
            queryServerStatus().queryProperties()->removeProp("Cluster");
        }
        queryServerStatus().commitProperties();
    }
    catch (IException *e)
    {
        FLLOG(MCexception(e), thorJob, e, "WARNING: Failed to set wuid in SDS:");
        e->Release();
    }
    catch (CATCHALL)
    {
        FLLOG(MCerror, thorJob, "WARNING: Failed to set wuid in SDS: Unknown error");
    }
}

void CJobManager::replyException(CJobMaster &job, IException *e)
{
    SCMStringBuffer wuid;
    job.queryWorkUnit().getWuid(wuid);
    reply(&job.queryWorkUnit(), wuid.str(), e, job.queryAgentEp(), false);
}

void CJobManager::reply(IConstWorkUnit *workunit, const char *wuid, IException *e, const SocketEndpoint &agentep, bool allDone)
{
    CriticalBlock b(replyCrit);
    if (!conversation) 
        return;
    StringBuffer s;
    if (e) {
        s.append("Posting exception: ");
        e->errorMessage(s);
    }
    else
        s.append("Posting OK");
    s.append(" to agent ");
    agentep.getUrlStr(s);
    s.append(" for workunit(").append(wuid).append(")");
    PROGLOG("%s", s.str());
    MemoryBuffer replyMb;
    workunit->forceReload();
    if (!allDone && (WUActionPause == workunit->getAction() || WUActionPauseNow == workunit->getAction()))
    {
        replyMb.append((unsigned)DAMP_THOR_REPLY_PAUSED);
        if (e)
        {
            // likely if WUActionPauseNow, shouldn't happen if WUActionPause
            EXCLOG(e, "Exception at time of pause");
            replyMb.append(true);
            serializeException(e, replyMb);
        }
        else
            replyMb.append(false);
    }
    else if (e)
    {
        IThorException *te = QUERYINTERFACE(e, IThorException);
        if (te && TE_WorkUnitAborting == te->errorCode())
            replyMb.append((unsigned)DAMP_THOR_REPLY_ABORT);
        else
            replyMb.append((unsigned)DAMP_THOR_REPLY_ERROR);
        serializeException(e, replyMb);
    }
    else
        replyMb.append((unsigned)DAMP_THOR_REPLY_GOOD);
    if (!conversation->send(replyMb)) {
        s.clear();
        ERRLOG("Failed to reply to agent %s",agentep.getUrlStr(s).str());
    }
    conversation.clear();
    handlingConversation = false;

    Owned<IRemoteConnection> conn = querySDS().connect("/Environment", myProcessSession(), RTM_LOCK_READ, MEDIUMTIMEOUT);
    if (checkThorNodeSwap(globals->queryProp("@name"),e?wuid:NULL,(unsigned)-1))
        abortThor(e,false);
}

bool CJobManager::executeGraph(IConstWorkUnit &workunit, const char *graphName, const SocketEndpoint &agentEp)
{
    {
        Owned<IWorkUnit> wu = &workunit.lock();
        wu->setTracingValue("ThorBuild", BUILD_TAG);
        StringBuffer log, logUrl;
        logHandler->getLogName(log);
        createUNCFilename(log, logUrl, false);
        wu->addProcess("Thor", globals->queryProp("@name"), logUrl.str());
        StringBuffer tsStr("Thor - ");
        wu->setTimeStamp(tsStr.append(graphName).str(), GetCachedHostName(), "Started");
    }
    Owned<IException> exception;
    SCMStringBuffer wuid;
    workunit.forceReload();
    workunit.getWuid(wuid);
    const char *totalTimeStr = "Total thor time";
    unsigned startTime = msTick();
    unsigned totalTimeMs = workunit.getTimerDuration(totalTimeStr, NULL);

    Owned<IConstWUQuery> query = workunit.getQuery(); 
    SCMStringBuffer soName;
    query->getQueryDllName(soName);
    unsigned version = query->getQueryDllCrc();
    query.clear();

    StringBuffer soPath;
    globals->getProp("@query_so_dir", soPath);
    StringBuffer compoundPath;
    compoundPath.append(soPath.str());
    soPath.append(soName.str());
    getCompoundQueryName(compoundPath, soName.str(), version);
    bool sendSo = false;
    if (querySoCache.isAvailable(compoundPath.str()))
        PROGLOG("Using existing local dll: %s", compoundPath.str()); // It is assumed if present here then _still_ present on slaves from previous send.
    else
    {
        MemoryBuffer file;
        queryDllServer().getDll(soName.str(), file);
        PROGLOG("Saving dll: %s", compoundPath.str());
        OwnedIFile out = createIFile(compoundPath.str());
        try
        {
            out->setCreateFlags(S_IRWXU); 
            OwnedIFileIO io = out->open(IFOcreate);
            io->write(0, file.length(), file.toByteArray());
            io.clear();
        }
        catch (IException *e)
        {
            FLLOG(MCexception(e), thorJob, e, "Failed to write query dll - ignoring!");
            e->Release();   
        }
        sendSo = globals->getPropBool("Debug/@dllsToSlaves", true);
    }

    SCMStringBuffer user, eclstr;
    workunit.getUser(user);

    PROGLOG("Started wuid=%s, user=%s, graph=%s\n", wuid.str(), user.str(), graphName);

    PROGLOG("Query %s loaded", compoundPath.str());
    Owned<IConstWUGraph> graph = workunit.getGraph(graphName);
    Owned<IPropertyTree> graphXGMML = graph->getXGMMLTree(false);
    Owned<CJobMaster> job = createThorGraph(graphName, graphXGMML, workunit, compoundPath.str(), sendSo, agentEp);
    PROGLOG("Graph %s created", graphName);
    graphXGMML.clear();
    graph.clear();
    PROGLOG("Running graph=%s", job->queryGraphName());
    addJob(*job);
    bool allDone = false;
    try
    {
        struct CounterBlock
        {
            atomic_t &counter;
            CounterBlock(atomic_t &_counter) : counter(_counter) { atomic_inc(&counter); }
            ~CounterBlock() { atomic_dec(&counter); }
        } cBlock(activeTasks);

        {
            Owned<IWorkUnit> wu = &workunit.lock();
            wu->setState(WUStateRunning);
            VStringBuffer version("%d.%d", THOR_VERSION_MAJOR, THOR_VERSION_MINOR);
            wu->setDebugValue("ThorVersion", version.str(), true);
        }

        SCMStringBuffer wuid, clusterName;
        setWuid(workunit.getWuid(wuid).str(), workunit.getClusterName(clusterName).str());

        allDone = job->go();

        Owned<IWorkUnit> wu = &workunit.lock();
        unsigned graphTimeMs = msTick()-startTime;
        StringBuffer graphTimeStr;
        formatGraphTimerLabel(graphTimeStr, graphName);
        wu->setTimerInfo(graphTimeStr, NULL, graphTimeMs, 1, 0);
        wu->setTimerInfo(totalTimeStr, NULL, totalTimeMs+graphTimeMs, 1, 0);
        StringBuffer tsStr("Thor - ");
        wu->setTimeStamp(tsStr.append(graphName).str(), GetCachedHostName(), "Finished");
        
        removeJob(*job);
    }
    catch (IException *)
    {
        removeJob(*job);
        setWuid(NULL);
        throw;
    }
    job.clear();
    PROGLOG("Finished wuid=%s, graph=%s", wuid.str(), graphName);

    setWuid(NULL);
    return allDone;
}

void CJobManager::addCachedSo(const char *name)
{
    querySoCache.add(name);
}

static int exitCode = -1;
void setExitCode(int code) { exitCode = code; }
int queryExitCode() { return exitCode; }

static unsigned aborting = 99;
void abortThor(IException *e, bool abortCurrentJob)
{
    if (-1 == queryExitCode()) setExitCode(1);
    Owned<CJobManager> jM = ((CJobManager *)getJobManager());
    if (0 == aborting)
    {
        aborting = 1;
        if (!e) e = MakeThorException(TE_AbortException, "THOR ABORT");
        EXCLOG(e,"abortThor");
        LOG(MCdebugProgress, thorJob, "abortThor called");
        if (jM)
            jM->stop();
    }
    if (2 > aborting && abortCurrentJob)
    {
        aborting = 2;
        LOG(MCdebugProgress, thorJob, "aborting any current active job");
        if (jM)
            jM->fireException(e);
    }
}

#define DEFAULT_VERIFYDALI_POLL 5*60 // secs
class CDaliConnectionValidator : public CSimpleInterface, implements IThreaded
{
    bool stopped;
    unsigned pollDelay;
    Semaphore poll;
    CThreaded threaded;
public:
    CDaliConnectionValidator(unsigned _pollDelay) : threaded("CDaliConnectionValidator") { pollDelay = _pollDelay*1000; stopped = false; threaded.init(this); }
    ~CDaliConnectionValidator() { stop(); threaded.join(); }
    void main()
    {
        loop
        {
            poll.wait(pollDelay);
            if (stopped) break;
            if (!verifyCovenConnection(pollDelay)) // use poll delay time for verify connection timeout
            {
                abortThor(MakeThorOperatorException(TE_AbortException, "Detected lost connectivity with dali server, aborting thor"));
                break;
            }
        }
    }
    void stop()
    {
        stopped = true;
        poll.signal();
    }
};

static CSDSServerStatus *serverStatus = NULL;
CSDSServerStatus &queryServerStatus() { return *serverStatus; }

CSDSServerStatus &openThorServerStatus()
{
    assertex(!serverStatus);
    serverStatus = new CSDSServerStatus("ThorMaster");
    return *serverStatus;
}
void closeThorServerStatus()
{
    if (serverStatus)
    {
        delete serverStatus;
        serverStatus = NULL;
    }
}

void thorMain(ILogMsgHandler *logHandler)
{
    aborting = 0;
    unsigned multiThorMemoryThreshold = globals->getPropInt("@multiThorMemoryThreshold")*0x100000;
    try
    {
        Owned<CDaliConnectionValidator> daliConnectValidator = new CDaliConnectionValidator(globals->getPropInt("@verifyDaliConnectionInterval", DEFAULT_VERIFYDALI_POLL));
        Owned<ILargeMemLimitNotify> notify;
        if (multiThorMemoryThreshold) {
            StringBuffer ngname;
            if (!globals->getProp("@multiThorResourceGroup",ngname))
                globals->getProp("@nodeGroup",ngname);
            if (ngname.length()) {
                notify.setown(createMultiThorResourceMutex(ngname.str(),serverStatus));
                setMultiThorMemoryNotify(multiThorMemoryThreshold,notify);
                PROGLOG("Multi-Thor resource limit for %s set to %"I64F"d",ngname.str(),(__int64)multiThorMemoryThreshold);
            }   
            else
                multiThorMemoryThreshold = 0;
        }
        initFileManager();
        CThorResourceMaster masterResource;
        setIThorResource(masterResource);

        Owned<CJobManager> jobManager = new CJobManager(logHandler);
        try {
            LOG(MCdebugProgress, thorJob, "Listening for graph");
            jobManager->run();
        }
        catch (IException *e)
        {
            EXCLOG(e, NULL);
            throw;
        }
    }
    catch (IException *e) 
    {
        FLLOG(MCexception(e), thorJob, e,"ThorMaster");
        e->Release();
    }
    if (multiThorMemoryThreshold)
        setMultiThorMemoryNotify(0,NULL);
}
