/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2015 HPCC Systems®.

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
############################################################################## */

#include "platform.h"
#include "jlib.hpp"
#include "jthread.hpp"
#include "jregexp.hpp"

#include "wujobq.hpp"

#include "ccd.hpp"
#include "ccdcontext.hpp"
#include "ccdlistener.hpp"
#include "ccddali.hpp"
#include "ccdquery.hpp"
#include "ccdqueue.ipp"
#include "ccdsnmp.hpp"
#include "ccdstate.hpp"

//================================================================================================================================

class ProtocolListener : public Thread, implements IRoxieListener, implements IThreadFactory
{
public:
    IMPLEMENT_IINTERFACE;
    ProtocolListener(IHpccProtocolMsgSink *_sink, unsigned _poolSize, bool _suspended) : Thread("RoxieListener"), sink(_sink)
    {
        running = false;
        suspended = _suspended;
        poolSize = _poolSize;
        threadsActive = 0;
        maxThreadsActive = 0;
    }
    virtual CriticalSection &getActiveCrit()
    {
        return activeCrit;
    }
    virtual bool getIsSuspended()
    {
        return suspended;
    }
    virtual unsigned getActiveThreadCount()
    {
        return threadsActive;
    }
    virtual unsigned getPoolSize()
    {
        return poolSize;
    }
    virtual unsigned getMaxActiveThreads()
    {
        return maxThreadsActive;
    }
    virtual void setMaxActiveThreads(unsigned val)
    {
        maxThreadsActive=val;
    }
    virtual void incActiveThreadCount()
    {
        threadsActive++;
    }
    virtual void decActiveThreadCount()
    {
        threadsActive--;
    }


    static void updateAffinity()
    {
#ifdef CPU_ZERO
        if (sched_getaffinity(0, sizeof(cpu_set_t), &cpuMask))
        {
            if (traceLevel)
                DBGLOG("Unable to get CPU affinity - thread affinity settings will be ignored");
            cpuCores = 0;
            lastCore = 0;
            CPU_ZERO(&cpuMask);
        }
        else
        {
#if __GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 6)
            cpuCores = CPU_COUNT(&cpuMask);
#else
            cpuCores = 0;
            unsigned setSize = CPU_SETSIZE;
            while (setSize--)
            {
                if (CPU_ISSET(setSize, &cpuMask))
                    ++cpuCores;
            }
#endif /* GLIBC */
            if (traceLevel)
                traceAffinity(&cpuMask);
        }
#endif
    }

    virtual void start()
    {
        // Note we allow a few additional threads than requested - these are the threads that return "Too many active queries" responses
        pool.setown(createThreadPool("RoxieSocketWorkerPool", this, NULL, poolSize+5, INFINITE));
        assertex(!running);
        Thread::start();
        started.wait();
    }

    virtual bool stop(unsigned timeout)
    {
        if (running)
        {
            running = false;
            join();
            Release();
        }
        return pool->joinAll(false, timeout);
    }

    void reportBadQuery(const char *name, const IContextLogger &logctx)
    {
        // MORE - may want to put in a mechanism to avoid swamping SNMP with bad query reports if someone kicks off a thor job with a typo....
        logctx.logOperatorException(NULL, NULL, 0, "Unknown query %s", name);
    }

    void checkWuAccess(bool isBlind)
    {
        // Could do some LDAP access checking here (via Dali?)
    }

    void checkAccess(IpAddress &peer, const char *queryName, const char *queryText, bool isBlind)
    {
        sink->checkAccess(peer, queryName, queryText, isBlind);
    }

    virtual bool suspend(bool suspendIt)
    {
        CriticalBlock b(activeCrit);
        bool ret = suspended;
        suspended = suspendIt;
        return ret;
    }

    virtual void addAccess(bool allow, bool allowBlind, const char *ip, const char *mask, const char *query, const char *errorMsg, int errorCode)
    {
        sink->addAccess(allow, allowBlind, ip, mask, query, errorMsg, errorCode);
    }

    void queryAccessInfo(StringBuffer &info)
    {
        sink->queryAccessInfo(info);
    }

    void setThreadAffinity(int numCores)
    {
#ifdef CPU_ZERO
        // Note - strictly speaking not threadsafe but any race conditions are (a) unlikely and (b) harmless
        if (cpuCores)
        {
            if (numCores > 0 && numCores < cpuCores)
            {
                cpu_set_t threadMask;
                CPU_ZERO(&threadMask);
                unsigned cores = 0;
                unsigned offset = lastCore;
                unsigned core;
                for (core = 0; core < CPU_SETSIZE; core++)
                {
                    unsigned useCore = (core + offset) % CPU_SETSIZE;
                    if (CPU_ISSET(useCore, &cpuMask))
                    {
                        CPU_SET(useCore, &threadMask);
                        cores++;
                        if (cores == numCores)
                        {
                            lastCore = useCore+1;
                            break;
                        }
                    }
                }
                if (traceLevel > 3)
                    traceAffinity(&threadMask);
                pthread_setaffinity_np(GetCurrentThreadId(), sizeof(cpu_set_t), &threadMask);
            }
            else
            {
                if (traceLevel > 3)
                    traceAffinity(&cpuMask);
                pthread_setaffinity_np(GetCurrentThreadId(), sizeof(cpu_set_t), &cpuMask);
            }
        }
#endif
    }

protected:
    unsigned poolSize;
    bool running;
    bool suspended;
    Semaphore started;
    Owned<IThreadPool> pool;

    unsigned threadsActive;
    unsigned maxThreadsActive;
    CriticalSection activeCrit;
    Linked<IHpccProtocolMsgSink> sink;

#ifdef CPU_ZERO
    static cpu_set_t cpuMask;
    static unsigned cpuCores;
    static unsigned lastCore;

private:
    static void traceAffinity(cpu_set_t *mask)
    {
        StringBuffer trace;
        for (unsigned core = 0; core < CPU_SETSIZE; core++)
        {
            if (CPU_ISSET(core, mask))
                trace.appendf(",%d", core);
        }
        if (trace.length())
            DBGLOG("Process affinity is set to use core(s) %s", trace.str()+1);
    }
#endif

};

#ifdef CPU_ZERO
cpu_set_t ProtocolListener::cpuMask;
unsigned ProtocolListener::cpuCores;
unsigned ProtocolListener::lastCore;
#endif


interface IRoxieListenerx : extends IInterface
{
    virtual void start() = 0;
    //virtual bool stop(unsigned timeout) = 0;
    //virtual void stopListening() = 0;
    //virtual void disconnectQueue() = 0;
    virtual void addAccess(bool allow, bool allowBlind, const char *ip, const char *mask, const char *query, const char *errMsg, int errCode) = 0;
    //virtual unsigned queryPort() const = 0;
    //virtual const SocketEndpoint &queryEndpoint() const = 0;
    virtual bool suspend(bool suspendIt) = 0;

    //virtual void runOnce(const char *query) = 0;
};

class ProtocolSocketListener : public ProtocolListener
{
    unsigned port;
    unsigned listenQueue;
    Owned<ISocket> socket;
    SocketEndpoint ep;

public:
    ProtocolSocketListener(IHpccProtocolMsgSink *_sink, unsigned _port, unsigned _poolSize, unsigned _listenQueue, bool _suspended)
      : ProtocolListener(_sink, _poolSize, _suspended)
    {
        port = _port;
        listenQueue = _listenQueue;
        ep.set(port, queryHostIP());
    }

    IHpccProtocolMsgSink *queryMsgSink(){return sink.get();}

    virtual bool stop(unsigned timeout)
    {
        if (socket)
            socket->cancel_accept();
        return ProtocolListener::stop(timeout);
    }

    virtual void disconnectQueue()
    {
        // This is for dali queues only
    }

    virtual void stopListening()
    {
        // Not threadsafe, but we only call this when generating a core file... what's the worst that can happen?
        try
        {
            DBGLOG("Closing listening socket %d", port);
            socket.clear();
            DBGLOG("Closed listening socket %d", port);
        }
        catch(...)
        {
        }
    }

    virtual void runOnce(const char *query);

    virtual int run()
    {
        DBGLOG("ProtocolSocketListener (%d threads) listening to socket on port %d", poolSize, port);
        socket.setown(ISocket::create(port, listenQueue));
        running = true;
        started.signal();
        while (running)
        {
            ISocket *client = socket->accept(true);
            if (client)
            {
                client->set_linger(-1);
                pool->start(client);
            }
        }
        DBGLOG("ProtocolSocketListener closed query socket");
        return 0;
    }

    virtual IPooledThread *createNew();

    virtual const SocketEndpoint &queryEndpoint() const
    {
        return ep;
    }

    virtual unsigned queryPort() const
    {
        return port;
    }
};

class RoxieQueryWorkerTodoMakeCommon : public CInterface, implements IPooledThread
{
public:
    IMPLEMENT_IINTERFACE;

    RoxieQueryWorkerTodoMakeCommon(ProtocolListener *_pool)
    {
        pool = _pool;
        qstart = msTick();
        time(&startTime);
    }

    //  interface IPooledThread
    virtual void init(void *)
    {
        qstart = msTick();
        time(&startTime);
    }

    virtual bool canReuse()
    {
        return true;
    }

    virtual bool stop()
    {
        ERRLOG("RoxieQueryWorker stopped with queries active");
        return true;
    }

protected:
    ProtocolListener *pool;
    unsigned qstart;
    time_t startTime;

};

//================================================================================================================

class CRoxieNativeProtocolWriter : public CInterface, implements IHpccProtocol
{
protected:
    SafeSocket *client;
    CriticalSection resultsCrit;
    IPointerArrayOf<FlushingStringBuffer> resultMap;

    StringAttr queryName;
    const IContextLogger &logctx;
    Owned<FlushingStringBuffer> probe;
    TextMarkupFormat mlFmt;
    PTreeReaderOptions xmlReadFlags;
    bool isRaw;
    bool isHTTP;
    bool isBlocked;
    bool trim;
    bool failed;


public:
    IMPLEMENT_IINTERFACE;
    CRoxieNativeProtocolWriter(const char *queryname, SafeSocket *_client, bool _isBlocked, TextMarkupFormat _mlFmt, bool _isRaw, bool _isHTTP, const IContextLogger &_logctx, PTreeReaderOptions _xmlReadFlags) :
        queryName(queryname), client(_client), isBlocked(_isBlocked), mlFmt(_mlFmt), isRaw(_isRaw), isHTTP(_isHTTP), logctx(_logctx), xmlReadFlags(_xmlReadFlags)
    {
    }
    virtual bool checkConnection()
    {
        return client->checkConnection();
    }
    virtual void sendHeartBeat()
    {
        client->sendHeartBeat(logctx);
    }
    virtual SafeSocket *querySafeSocket()
    {
        return client;
    }
    virtual FlushingStringBuffer *queryResult(unsigned sequence)
    {
        CriticalBlock procedure(resultsCrit);
        while (!resultMap.isItem(sequence))
            resultMap.append(NULL);
        FlushingStringBuffer *result = resultMap.item(sequence);
        if (!result)
        {
            if (mlFmt==MarkupFmt_JSON)
                result = new FlushingJsonBuffer(client, isBlocked, isHTTP, logctx);
            else
                result = new FlushingStringBuffer(client, isBlocked, mlFmt, isRaw, isHTTP, logctx);
            result->isSoap = isHTTP;
            result->trim = trim;
            result->queryName.set(queryName);
            resultMap.replace(result, sequence);
        }
        return result;
    }
    virtual FlushingStringBuffer *createFlushingBuffer()
    {
        return new FlushingStringBuffer(client, isBlocked, mlFmt, isRaw, isHTTP, logctx);
    }
    virtual IXmlWriter *startDataset(const char *name, unsigned sequence, const char *elementName, bool &appendRawData, unsigned writeFlags, bool _extend = false, const IProperties *xmlns=NULL)
    {
        FlushingStringBuffer *response = queryResult(sequence);
        if (response)
        {
            appendRawData = response->isRaw;
            response->startDataset(elementName, name, sequence, _extend, xmlns);
            if (response->mlFmt==MarkupFmt_XML || response->mlFmt==MarkupFmt_JSON)
            {
                if (response->mlFmt==MarkupFmt_JSON)
                    writeFlags |= XWFnoindent;
                Owned<IXmlWriter> xmlwriter = createIXmlWriterExt(writeFlags, 1, response, (response->mlFmt==MarkupFmt_JSON) ? WTJSON : WTStandard);
                xmlwriter->outputBeginArray(DEFAULTXMLROWTAG);
                return xmlwriter.getClear();
            }
        }
        return NULL;
    }
    virtual void finalizeXmlRow(unsigned sequence)
    {
        if (mlFmt==MarkupFmt_XML || mlFmt==MarkupFmt_JSON)
        {
            FlushingStringBuffer *r = queryResult(sequence);
            if (r)
            {
                r->incrementRowCount();
                r->flush(false);
            }
        }
    }

    virtual void appendRawRow(unsigned sequence, unsigned len, const char *data)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->append(len, data);
            r->incrementRowCount();
            r->flush(false);
        }
    }
    virtual void appendSimpleRow(unsigned sequence, const char *str)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
            r->append(str);
    }

    virtual void setResultBool(const char *name, unsigned sequence, bool value)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            if (isRaw)
                r->append(sizeof(value), (char *)&value);
            else
                r->append(value ? "true" : "false");
        }
    }
    virtual void setResultData(const char *name, unsigned sequence, int len, const void * data)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            r->encodeData(data, len);
        }
    }
    virtual void setResultRaw(const char *name, unsigned sequence, int len, const void * data)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            if (isRaw)
                r->append(len, (const char *) data);
            else
                UNIMPLEMENTED;
        }
    }
    virtual void setResultSet(const char *name, unsigned sequence, bool isAll, size32_t len, const void * data, ISetToXmlTransformer * transformer)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            if (isRaw)
                r->append(len, (char *)data);
            else if (mlFmt==MarkupFmt_XML)
            {
                assertex(transformer);
                CommonXmlWriter writer(xmlReadFlags|XWFnoindent, 0);
                transformer->toXML(isAll, len, (byte *)data, writer);
                r->append(writer.str());
            }
            else if (mlFmt==MarkupFmt_JSON)
            {
                assertex(transformer);
                CommonJsonWriter writer(xmlReadFlags|XWFnoindent, 0);
                transformer->toXML(isAll, len, (byte *)data, writer);
                r->append(writer.str());
            }
            else
            {
                assertex(transformer);
                r->append('[');
                if (isAll)
                    r->appendf("*]");
                else
                {
                    SimpleOutputWriter x;
                    transformer->toXML(isAll, len, (const byte *) data, x);
                    r->appendf("%s]", x.str());
                }
            }
        }
    }

    virtual void setResultDecimal(const char *name, unsigned sequence, int len, int precision, bool isSigned, const void *val)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            if (isRaw)
                r->append(len, (char *)val);
            else
            {
                StringBuffer s;
                if (isSigned)
                    outputXmlDecimal(val, len, precision, NULL, s);
                else
                    outputXmlUDecimal(val, len, precision, NULL, s);
                r->append(s);
            }
        }
    }
    virtual void setResultInt(const char *name, unsigned sequence, __int64 value, unsigned size)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            if (isRaw)
            {
                r->startScalar(name, sequence);
                r->append(sizeof(value), (char *)&value);
            }
            else
                r->setScalarInt(name, sequence, value, size);
        }
    }

    virtual void setResultUInt(const char *name, unsigned sequence, unsigned __int64 value, unsigned size)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            if (isRaw)
            {
                r->startScalar(name, sequence);
                r->append(sizeof(value), (char *)&value);
            }
            else
                r->setScalarUInt(name, sequence, value, size);
        }
    }

    virtual void setResultReal(const char *name, unsigned sequence, double value)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            r->append(value);
        }
    }
    virtual void setResultString(const char *name, unsigned sequence, int len, const char * str)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            if (r->isRaw)
            {
                r->append(len, str);
            }
            else
            {
                r->encodeString(str, len);
            }
        }
    }
    virtual void setResultUnicode(const char *name, unsigned sequence, int len, UChar const * str)
    {
        FlushingStringBuffer *r = queryResult(sequence);
        if (r)
        {
            r->startScalar(name, sequence);
            if (r->isRaw)
            {
                r->append(len*2, (const char *) str);
            }
            else
            {
                rtlDataAttr buff;
                unsigned bufflen = 0;
                rtlUnicodeToCodepageX(bufflen, buff.refstr(), len, str, "utf-8");
                r->encodeString(buff.getstr(), bufflen, true); // output as UTF-8
            }
        }
    }
    virtual void setResultVarString(const char * name, unsigned sequence, const char * value)
    {
        setResultString(name, sequence, strlen(value), value);
    }
    virtual void setResultVarUnicode(const char * name, unsigned sequence, UChar const * value)
    {
        setResultUnicode(name, sequence, rtlUnicodeStrlen(value), value);
    }
    virtual void flush()
    {
        ForEachItemIn(seq, resultMap)
        {
            FlushingStringBuffer *result = resultMap.item(seq);
            if (result)
                result->flush(true);
        }
    }
    virtual void finalize(unsigned seqNo)
    {
        flush();
    }
    virtual void appendProbeGraph(const char *xml)
    {
        if (!xml)
        {
            if (probe)
                probe.clear();
            return;
        }
        if (!probe)
        {
            probe.setown(new FlushingStringBuffer(client, isBlocked, MarkupFmt_XML, false, isHTTP, logctx));
            probe->startDataset("_Probe", NULL, (unsigned) -1);  // initialize it
        }

        probe->append("\n");
        probe->append(xml);
    }

};

class CRoxieXmlProtocolWriter : public CRoxieNativeProtocolWriter
{
public:
    CRoxieXmlProtocolWriter(const char *queryname, SafeSocket *_client, bool _isBlocked, TextMarkupFormat _mlFmt, bool _isRaw, bool _isHTTP, const IContextLogger &_logctx, PTreeReaderOptions _xmlReadFlags) :
        CRoxieNativeProtocolWriter(queryname, _client, _isBlocked, _mlFmt, _isRaw, _isHTTP, _logctx, _xmlReadFlags)
    {
    }

    virtual void finalize(unsigned seqNo)
    {
        CriticalBlock b(resultsCrit);
        CriticalBlock b1(client->queryCrit());

        StringBuffer responseHead, responseTail;
        responseHead.append("<").append(queryName).append("Response");
        responseHead.append(" sequence=\"").append(seqNo).append("\"");
        responseHead.append(" xmlns=\"urn:hpccsystems:ecl:").appendLower(queryName.length(), queryName.str()).append("\">");
        responseHead.append("<Results><Result>");
        unsigned len = responseHead.length();
        client->write(responseHead.detach(), len, true);

        ForEachItemIn(seq, resultMap)
        {
            FlushingStringBuffer *result = resultMap.item(seq);
            if (result)
            {
                result->flush(true);
                for(;;)
                {
                    size32_t length;
                    void *payload = result->getPayload(length);
                    if (!length)
                        break;
                    client->write(payload, length, true);
                }
            }
        }

        responseTail.append("</Result></Results>");
        responseTail.append("</").append(queryName).append("Response>");
        len = responseTail.length();
        client->write(responseTail.detach(), len, true);
    }
};

class CRoxieJsonProtocolWriter : public CRoxieNativeProtocolWriter
{
public:
    CRoxieJsonProtocolWriter(const char *queryname, SafeSocket *_client, bool _isBlocked, TextMarkupFormat _mlFmt, bool _isRaw, bool _isHTTP, const IContextLogger &_logctx, PTreeReaderOptions _xmlReadFlags) :
        CRoxieNativeProtocolWriter(queryname, _client, _isBlocked, _mlFmt, _isRaw, _isHTTP, _logctx, _xmlReadFlags)
    {
    }

    virtual FlushingStringBuffer *createFlushingBuffer()
    {
        return new FlushingJsonBuffer(client, isBlocked, isHTTP, logctx);
    }

    virtual void finalize(unsigned seqNo)
    {
        CriticalBlock b(resultsCrit);
        CriticalBlock b1(client->queryCrit());

        StringBuffer responseHead, responseTail;
        appendfJSONName(responseHead, "%sResponse", queryName.get()).append(" {");
        appendJSONValue(responseHead, "sequence", seqNo);
        appendJSONName(responseHead, "Results").append(" {");

        unsigned len = responseHead.length();
        client->write(responseHead.detach(), len, true);

        bool needDelimiter = false;
        ForEachItemIn(seq, resultMap)
        {
            FlushingStringBuffer *result = resultMap.item(seq);
            if (result)
            {
                result->flush(true);
                for(;;)
                {
                    size32_t length;
                    void *payload = result->getPayload(length);
                    if (!length)
                        break;
                    if (needDelimiter)
                    {
                        StringAttr s(","); //write() will take ownership of buffer
                        size32_t len = s.length();
                        client->write((void *)s.detach(), len, true);
                        needDelimiter=false;
                    }
                    client->write(payload, length, true);
                }
                needDelimiter=true;
            }
        }

        responseTail.append("}}");
        len = responseTail.length();
        client->write(responseTail.detach(), len, true);
    }
};

class CHttpRequestAsyncFor : public CInterface, public CAsyncFor
{
private:
    const char *queryName, *queryText, *querySetName;
    const IContextLogger &logctx;
    IArrayOf<IPropertyTree> &requestArray;
    Linked<IHpccProtocolMsgSink> sink;
    Linked<IHpccProtocolMsgContext> msgctx;
    SafeSocket &client;
    HttpHelper &httpHelper;
    PTreeReaderOptions xmlReadFlags;
    unsigned &memused;
    unsigned &slaveReplyLen;
    CriticalSection crit;
    unsigned flags;

public:
    CHttpRequestAsyncFor(const char *_queryName, IHpccProtocolMsgSink *_sink, IHpccProtocolMsgContext *_msgctx, IArrayOf<IPropertyTree> &_requestArray,
            SafeSocket &_client, HttpHelper &_httpHelper, unsigned _flags, unsigned &_memused, unsigned &_slaveReplyLen, const char *_queryText, const IContextLogger &_logctx, PTreeReaderOptions _xmlReadFlags, const char *_querySetName)
    : sink(_sink), msgctx(_msgctx), requestArray(_requestArray), client(_client), httpHelper(_httpHelper), memused(_memused),
      slaveReplyLen(_slaveReplyLen), logctx(_logctx), xmlReadFlags(_xmlReadFlags), querySetName(_querySetName), flags(_flags)
    {
        queryName = _queryName;
        queryText = _queryText;
    }

    IMPLEMENT_IINTERFACE;

    virtual IXmlWriter *updateXmlResult(const char *name, unsigned sequence, const char *elementName, bool _extend = false, const IProperties *xmlns=NULL)
    {
        return NULL;
    }

    void onException(IException *E)
    {
        //if (!logctx.isBlind())
        //    logctx.CTXLOG("FAILED: %s", queryText);
        StringBuffer error("EXCEPTION: ");
        E->errorMessage(error);
        DBGLOG("%s", error.str());
        client.checkSendHttpException(httpHelper, E, queryName);
        E->Release();
    }

    void Do(unsigned idx)
    {
        try
        {
            IPropertyTree &request = requestArray.item(idx);
            Owned<CRoxieNativeProtocolWriter> protocol;
            if (httpHelper.queryContentFormat()==MarkupFmt_JSON)
                protocol.setown(new CRoxieJsonProtocolWriter(request.queryName(), &client, false, MarkupFmt_JSON, false, httpHelper.isHttp(), logctx, xmlReadFlags));
            else
                protocol.setown(new CRoxieXmlProtocolWriter(request.queryName(), &client, false, httpHelper.queryContentFormat(), false, httpHelper.isHttp(), logctx, xmlReadFlags));

            sink->query(msgctx, &request, protocol, flags, xmlReadFlags, querySetName, idx, memused, slaveReplyLen);
        }
        catch (WorkflowException * E)
        {
            onException(E);
        }
        catch (IException * E)
        {
            onException(E);
        }
        catch (...)
        {
            onException(MakeStringException(ROXIE_INTERNAL_ERROR, "Unknown exception"));
        }
    }
};

class QueryNameExtractor : public CInterface, implements IPTreeNotifyEvent
{
public:
    TextMarkupFormat mlFmt;
    StringAttr prefix;
    StringAttr name;
    unsigned headerDepth;
    bool isSoap;
    bool isRequestArray;
public:
    IMPLEMENT_IINTERFACE;

    QueryNameExtractor(TextMarkupFormat _mlFmt) : mlFmt(_mlFmt), headerDepth(0), isSoap(false), isRequestArray(false)
    {
    }
    void extractName(const char *msg, const IContextLogger &logctx, const char *peer, unsigned port)
    {
        Owned<IPullPTreeReader> parser;
        if (mlFmt==MarkupFmt_JSON)
            parser.setown(createPullJSONStringReader(msg, *this));
        else if (mlFmt==MarkupFmt_XML)
            parser.setown(createPullXMLStringReader(msg, *this));
        if (!parser)
            return;
        while (name.isEmpty() && parser->next());
        if (name.isEmpty())
        {
            const char *fmt = mlFmt==MarkupFmt_XML ? "XML" : "JSON";
            IException *E = MakeStringException(-1, "ERROR: Invalid %s received from %s:%d - %s queryName not found", fmt, peer, port, msg);
            logctx.logOperatorException(E, __FILE__, __LINE__, "Invalid query %s", fmt);
            throw E;
        }
        String nameStr(name.get());
        if (nameStr.endsWith("RequestArray"))
        {
            isRequestArray = true;
            name.set(nameStr.str(), nameStr.length() - strlen("RequestArray"));
        }
        else if (nameStr.endsWith("Request"))
        {
            name.set(nameStr.str(), nameStr.length() - strlen("Request"));
        }
    }
    virtual void beginNode(const char *tag, offset_t startOffset)
    {
        if (streq(tag, "__object__"))
            return;
        const char *local = strchr(tag, ':');
        if (local)
            local++;
        else
            local = tag;
        if (mlFmt==MarkupFmt_XML)
        {
            if (!isSoap && streq(local, "Envelope"))
            {
                isSoap=true;
                return;
            }
            if (isSoap && streq(local, "Header"))
            {
                headerDepth++;
                return;
            }
            if (isSoap && !headerDepth && streq(local, "Body"))
                return;
        }
        if (!headerDepth)
        {
            name.set(local);
            if (tag!=local)
                prefix.set(tag, local-tag-1);
        }
    }
    virtual void newAttribute(const char *name, const char *value){}
    virtual void beginNodeContent(const char *tag){}
    virtual void endNode(const char *tag, unsigned length, const void *value, bool binary, offset_t endOffset)
    {
        if (headerDepth && streq(tag, "Header"))
            headerDepth--;
    }

};


class RoxieSocketWorker : public RoxieQueryWorkerTodoMakeCommon
{
    Owned<SafeSocket> client;
    Owned<CDebugCommandHandler> debugCmdHandler;
    SocketEndpoint ep;
    Owned<IHpccProtocolMsgSink> sink;

public:
    IMPLEMENT_IINTERFACE;

    RoxieSocketWorker(ProtocolSocketListener *_pool, SocketEndpoint &_ep)
        : RoxieQueryWorkerTodoMakeCommon(_pool), ep(_ep)
    {
        sink.set(_pool->queryMsgSink());
    }

    //  interface IPooledThread
    virtual void init(void *_r)
    {
        client.setown(new CSafeSocket((ISocket *) _r));
        RoxieQueryWorkerTodoMakeCommon::init(_r);
    }

    virtual void main()
    {
        doMain("");
    }

    virtual void runOnce(const char *query)
    {
        doMain(query);
    }

private:
    static void sendHttpServerTooBusy(SafeSocket &client, const IContextLogger &logctx)
    {
        StringBuffer message;

        message.append("HTTP/1.0 503 Server Too Busy\r\n\r\n");
        message.append("Server too busy, please try again later");

        StringBuffer err("Too many active queries");  // write out Too many active queries - make searching for this error consistent
        if (!trapTooManyActiveQueries)
        {
            err.appendf("  %s", message.str());
            logctx.CTXLOG("%s", err.str());
        }
        else
        {
            IException *E = MakeStringException(ROXIE_TOO_MANY_QUERIES, "%s", err.str());
            logctx.logOperatorException(E, __FILE__, __LINE__, "%s", message.str());
            E->Release();
        }

        try
        {
            client.write(message.str(), message.length());
        }
        catch (IException *E)
        {
            logctx.logOperatorException(E, __FILE__, __LINE__, "Exception caught in sendHttpServerTooBusy");
            E->Release();
        }
        catch (...)
        {
            logctx.logOperatorException(NULL, __FILE__, __LINE__, "sendHttpServerTooBusy write failed (Unknown exception)");
        }
    }

    void sanitizeQuery(Owned<IPropertyTree> &queryPT, StringAttr &queryName, StringBuffer &saniText, HttpHelper &httpHelper, const char *&uid, bool &isRequest, bool &isRequestArray, bool &isBlind, bool &isDebug)
    {
        if (queryPT)
        {
            queryName.set(queryPT->queryName());
            isRequest = false;
            isRequestArray = false;
            if (httpHelper.isHttp())
            {
                if (httpHelper.queryContentFormat()==MarkupFmt_JSON)
                {
                    if (strieq(queryName, "__object__"))
                    {
                        queryPT.setown(queryPT->getPropTree("*[1]"));
                        queryName.set(queryPT->queryName());
                        isRequest = true;
                        if (!queryPT)
                            throw MakeStringException(ROXIE_DATA_ERROR, "Malformed JSON request (missing Body)");
                    }
                    else if (strieq(queryName, "__array__"))
                        throw MakeStringException(ROXIE_DATA_ERROR, "JSON request array not implemented");
                    else
                        throw MakeStringException(ROXIE_DATA_ERROR, "Malformed JSON request");
                }
                else
                {
                    if (strieq(queryName, "envelope"))
                        queryPT.setown(queryPT->getPropTree("Body/*"));
                    else if (!strnicmp(httpHelper.queryContentType(), "application/soap", strlen("application/soap")))
                        throw MakeStringException(ROXIE_DATA_ERROR, "Malformed SOAP request");
                    else
                        httpHelper.setUseEnvelope(false);
                    if (!queryPT)
                        throw MakeStringException(ROXIE_DATA_ERROR, "Malformed SOAP request (missing Body)");
                    String reqName(queryPT->queryName());
                    queryPT->removeProp("@xmlns:m");

                    // following code is moved from main() - should be no performance hit
                    String requestString("Request");
                    String requestArrayString("RequestArray");

                    if (reqName.endsWith(requestArrayString))
                    {
                        isRequestArray = true;
                        queryName.set(reqName.str(), reqName.length() - requestArrayString.length());
                    }
                    else if (reqName.endsWith(requestString))
                    {
                        isRequest = true;
                        queryName.set(reqName.str(), reqName.length() - requestString.length());
                    }
                    else
                        queryName.set(reqName.str());

                    queryPT->renameProp("/", queryName.get());  // reset the name of the tree
                }
            }

            // convert to XML with attribute values in single quotes - makes replaying queries easier
            uid = queryPT->queryProp("@uid");
            if (!uid)
                uid = "-";
            isBlind = queryPT->getPropBool("@blind", false) || queryPT->getPropBool("_blind", false);
            isDebug = queryPT->getPropBool("@debug") || queryPT->getPropBool("_Probe", false);
            toXML(queryPT, saniText, 0, isBlind ? (XML_SingleQuoteAttributeValues | XML_Sanitize) : XML_SingleQuoteAttributeValues);
        }
        else
            throw MakeStringException(ROXIE_DATA_ERROR, "Malformed request");
    }
    void parseQueryPTFromString(Owned<IPropertyTree> &queryPT, HttpHelper &httpHelper, const char *text, PTreeReaderOptions options)
    {
        if (strieq(httpHelper.queryContentType(), "application/json"))
            queryPT.setown(createPTreeFromJSONString(text, ipt_caseInsensitive, options));
        else
            queryPT.setown(createPTreeFromXMLString(text, ipt_caseInsensitive, options));
    }

    void doMain(const char *runQuery)
    {
        StringBuffer rawText(runQuery);
        unsigned priority = (unsigned) -2;
        unsigned memused = 0;
        IpAddress peer;
        bool continuationNeeded = false;
        bool isStatus = false;
readAnother:
        unsigned slavesReplyLen = 0;
        StringArray allTargets;
        sink->getTargetNames(allTargets);
        HttpHelper httpHelper(&allTargets);
        try
        {
            if (client)
            {
                client->querySocket()->getPeerAddress(peer);
                if (!client->readBlock(rawText, WAIT_FOREVER, &httpHelper, continuationNeeded, isStatus, maxBlockSize))
                {
                    if (traceLevel > 8)
                    {
                        StringBuffer b;
                        DBGLOG("No data reading query from socket");
                    }
                    client.clear();
                    return;
                }
            }
            if (continuationNeeded)
            {
                qstart = msTick();
                time(&startTime);
            }
        }
        catch (IException * E)
        {
            if (traceLevel > 0)
            {
                StringBuffer b;
                DBGLOG("Error reading query from socket: %s", E->errorMessage(b).str());
            }
            E->Release();
            client.clear();
            return;
        }

        bool isHTTP = httpHelper.isHttp();
        TextMarkupFormat mlFmt = isHTTP ? httpHelper.queryContentFormat() : MarkupFmt_XML;

        bool failed = false;
        unsigned protocolFlags = 0;

        Owned<IPropertyTree> queryPT;
        StringBuffer sanitizedText;
        StringBuffer peerStr;
        peer.getIpText(peerStr);
        const char *uid = "-";

        IHpccProtocolMsgContext *msgctx = sink->createMsgContext(startTime);
        IContextLogger &logctx = *msgctx->queryLogContext();

        StringAttr queryName;
        StringAttr queryPrefix;
        if (mlFmt==MarkupFmt_XML || mlFmt==MarkupFmt_JSON)
        {
            QueryNameExtractor extractor(mlFmt);
            extractor.extractName(rawText.str(), logctx, peerStr, ep.port);
            queryName.set(extractor.name);
            queryPrefix.set(extractor.prefix);
        }
        try
        {
            if (streq(queryPrefix.str(), "control"))
            {
                bool aclupdate = strieq(queryName, "aclupdate"); //ugly
                byte iptFlags = aclupdate ? ipt_caseInsensitive : 0;

                if (mlFmt==MarkupFmt_JSON)
                    queryPT.setown(createPTreeFromJSONString(rawText.str(), iptFlags, (PTreeReaderOptions)(ptr_ignoreWhiteSpace|ptr_ignoreNameSpaces)));
                else
                    queryPT.setown(createPTreeFromXMLString(rawText.str(), iptFlags, (PTreeReaderOptions)(ptr_ignoreWhiteSpace|ptr_ignoreNameSpaces)));

                FlushingStringBuffer response(client, false, MarkupFmt_XML, false, false, logctx);
                response.startDataset("Control", NULL, (unsigned) -1);

                StringBuffer reply;
                sink->control(msgctx, queryPT, rawText.str(), reply);
                if (mlFmt==MarkupFmt_JSON)
                {
                    Owned<IPropertyTree> convertPT = createPTreeFromXMLString(reply.str());
                    toJSON(convertPT, reply.clear()); //TODO: make control queries output using IXmlWriter instead of converting
                }
                response.append(reply.str());
                if (streq(queryName, "lock") || streq(queryName, "childlock"))
                    goto readAnother;
            }
            else if (isStatus)
            {
                client->write("OK", 2);
            }
            else
            {
                try
                {
                    parseQueryPTFromString(queryPT, httpHelper, rawText.str(), (PTreeReaderOptions)(defaultXmlReadFlags | ptr_ignoreNameSpaces));
                }
                catch (IException *E)
                {
                    logctx.logOperatorException(E, __FILE__, __LINE__, "Invalid XML received from %s:%d - %s", peerStr.str(), pool->queryPort(), rawText.str());
                    logctx.CTXLOG("ERROR: Invalid XML received from %s:%d - %s", peerStr.str(), pool->queryPort(), rawText.str());
                    throw;
                }

                bool isRequest = false;
                bool isRequestArray = false;
                bool isBlind = false;
                bool isDebug = false;

                sanitizeQuery(queryPT, queryName, sanitizedText, httpHelper, uid, isRequest, isRequestArray, isBlind, isDebug);
                pool->checkAccess(peer, queryName, sanitizedText, isBlind);

                if (isDebug)
                    msgctx->verifyAllowDebug();
                isBlind = msgctx->checkSetBlind(isBlind);

                if (msgctx->logFullQueries())
                {
                    StringBuffer soapStr;
                    (isRequest) ? soapStr.append("SoapRequest") : (isRequestArray) ? soapStr.append("SoapRequest") : soapStr.clear();
                    logctx.CTXLOG("%s %s:%d %s %s %s", isBlind ? "BLIND:" : "QUERY:", peerStr.str(), pool->queryPort(), uid, soapStr.str(), sanitizedText.str());
                }
                if (strieq(queryPrefix.str(), "debug"))
                {
                    FlushingStringBuffer response(client, false, MarkupFmt_XML, false, isHTTP, logctx);
                    response.startDataset("Debug", NULL, (unsigned) -1);
                    CommonXmlWriter out(0, 1);
                    sink->debug(msgctx, uid, queryPT, rawText.str(), out);
                    response.append(out.str());
                }

                ActiveQueryLimiter l(pool);
                if (!l.accepted)
                {
                    if (isHTTP)
                    {
                        sendHttpServerTooBusy(*client, logctx);
                        logctx.CTXLOG("FAILED: %s", sanitizedText.str());
                        logctx.CTXLOG("EXCEPTION: Too many active queries");
                    }
                    else
                    {
                        IException *e = MakeStringException(ROXIE_TOO_MANY_QUERIES, "Too many active queries");
                        if (msgctx->trapTooManyActiveQueries())
                            logctx.logOperatorException(e, __FILE__, __LINE__, NULL);
                        throw e;
                    }
                }
                else
                {
                    StringBuffer querySetName;
                    if (isHTTP)
                    {
                        client->setHttpMode(queryName, isRequestArray, httpHelper);
                        querySetName.set(httpHelper.queryTarget());
                        if (querySetName.length())
                        {
                            const char *target = targetAliases->queryProp(querySetName.str()); //adf
                            if (target)
                                querySetName.set(target);
                        }
                    }
                    if (msgctx->initQuery(querySetName, queryName))
                    {
                        int bindCores = queryPT->getPropInt("@bindCores", msgctx->getBindCores());
                        if (bindCores > 0)
                            pool->setThreadAffinity(bindCores);
                        bool stripWhitespace = msgctx->getStripWhitespace();
                        stripWhitespace = queryPT->getPropBool("_stripWhitespaceFromStoredDataset", stripWhitespace);
                        PTreeReaderOptions xmlReadFlags = (PTreeReaderOptions)((defaultXmlReadFlags & ~ptr_ignoreWhiteSpace) |
                                                                           (stripWhitespace ? ptr_ignoreWhiteSpace : ptr_none));
                        if (xmlReadFlags != defaultXmlReadFlags)
                        {
                            // we need to reparse input xml, as global whitespace setting has been overridden
                            parseQueryPTFromString(queryPT, httpHelper, rawText.str(), (PTreeReaderOptions)(xmlReadFlags | ptr_ignoreNameSpaces));
                            sanitizeQuery(queryPT, queryName, sanitizedText, httpHelper, uid, isRequest, isRequestArray, isBlind, isDebug);
                        }
                        IArrayOf<IPropertyTree> requestArray;
                        if (isHTTP)
                        {
                            mlFmt = httpHelper.queryContentFormat();
                            if (isRequestArray)
                            {
                                StringBuffer reqIterString;
                                reqIterString.append(queryName).append("Request");

                                Owned<IPropertyTreeIterator> reqIter = queryPT->getElements(reqIterString.str());
                                ForEach(*reqIter)
                                {
                                    IPropertyTree *fixedreq = createPTree(queryName, ipt_caseInsensitive);
                                    Owned<IPropertyTreeIterator> iter = reqIter->query().getElements("*");
                                    ForEach(*iter)
                                    {
                                        fixedreq->addPropTree(iter->query().queryName(), LINK(&iter->query()));
                                    }
                                    requestArray.append(*fixedreq);
                                }
                            }
                            else
                            {
                                IPropertyTree *fixedreq = createPTree(queryName, ipt_caseInsensitive);
                                Owned<IPropertyTreeIterator> iter = queryPT->getElements("*");
                                ForEach(*iter)
                                {
                                    fixedreq->addPropTree(iter->query().queryName(), LINK(&iter->query()));
                                }
                                requestArray.append(*fixedreq);
                            }
                            if (httpHelper.getTrim())
                                protocolFlags |= HPCC_PROTOCOL_TRIM;
                        }
                        else
                        {
                            const char *format = queryPT->queryProp("@format");
                            if (format)
                            {
                                if (stricmp(format, "raw") == 0)
                                {
                                    protocolFlags |= HPCC_PROTOCOL_NATIVE_RAW;
                                    mlFmt = MarkupFmt_Unknown;
                                    if (client != NULL)
                                        protocolFlags |= HPCC_PROTOCOL_BLOCKED;
                                }
                                else if (stricmp(format, "bxml") == 0)
                                {
                                    protocolFlags |= HPCC_PROTOCOL_BLOCKED;
                                }
                                else if (stricmp(format, "ascii") == 0)
                                {
                                    mlFmt = MarkupFmt_Unknown;
                                }
                                else if (stricmp(format, "xml") != 0) // xml is the default
                                    throw MakeStringException(ROXIE_INVALID_INPUT, "Unsupported format specified: %s", format);
                            }
                            if (queryPT->getPropBool("@trim", false))
                                protocolFlags |= HPCC_PROTOCOL_TRIM;
                            msgctx->setIntercept(queryPT->getPropBool("@log", false));
                            msgctx->setTraceLevel(queryPT->getPropInt("@traceLevel", logctx.queryTraceLevel()));
                        }

                        priority = msgctx->getQueryPriority(); //adf move
                        switch (priority)
                        {
                        case 0: loQueryStats.noteActive(); break;
                        case 1: hiQueryStats.noteActive(); break;
                        case 2: slaQueryStats.noteActive(); break;
                        }
                        unknownQueryStats.noteComplete();
                        combinedQueryStats.noteActive();

                        if (isHTTP)
                        {
                            CHttpRequestAsyncFor af(queryName, sink, msgctx, requestArray, *client, httpHelper, protocolFlags, memused, slavesReplyLen, sanitizedText, logctx, xmlReadFlags, querySetName);
                            af.For(requestArray.length(), numRequestArrayThreads);
                        }
                        else
                        {
                            Owned<CRoxieNativeProtocolWriter> writer;
                            if (protocolFlags & HPCC_PROTOCOL_NATIVE_RAW)
                                writer.setown(new CRoxieNativeProtocolWriter(queryPT->queryName(), client, false, mlFmt, false, true, logctx, xmlReadFlags));
                            else if (mlFmt==MarkupFmt_JSON)
                                writer.setown(new CRoxieJsonProtocolWriter(queryPT->queryName(), client, false, mlFmt, false, true, logctx, xmlReadFlags));
                            else
                                writer.setown(new CRoxieXmlProtocolWriter(queryPT->queryName(), client, false, mlFmt, false, true, logctx, xmlReadFlags));  //adf raw!

                            unsigned replyLen = 0;
                            client->write(&replyLen, sizeof(replyLen));

                            sink->query(msgctx, queryPT, writer, protocolFlags, xmlReadFlags, querySetName, 0, memused, slavesReplyLen);
                        }
                    }
                    else
                    {
                        pool->reportBadQuery(queryName.get(), logctx);
                        if (globalPackageSetManager->getActivePackageCount())
                        {
                            StringBuffer targetMsg;
                            const char *target = httpHelper.queryTarget();
                            if (target && *target)
                                targetMsg.append(", in target ").append(target);
                            throw MakeStringException(ROXIE_UNKNOWN_QUERY, "Unknown query %s%s", queryName.get(), targetMsg.str());
                        }
                        else
                            throw MakeStringException(ROXIE_NO_PACKAGES_ACTIVE, "Unknown query %s (no packages active)", queryName.get());
                    }
                }
            }
        }
        catch (WorkflowException * E)
        {
            failed = true;
            logctx.CTXLOG("FAILED: %s", sanitizedText.str());
            StringBuffer error;
            E->errorMessage(error);
            logctx.CTXLOG("EXCEPTION: %s", error.str());
            unsigned code = E->errorCode();
            if (QUERYINTERFACE(E, ISEH_Exception))
                code = ROXIE_INTERNAL_ERROR;
            else if (QUERYINTERFACE(E, IOutOfMemException))
                code = ROXIE_MEMORY_ERROR;
            if (client)
            {
                if (isHTTP)
                    client->checkSendHttpException(httpHelper, E, queryName);
                else
                    client->sendException("Roxie", code, error.str(), (protocolFlags & HPCC_PROTOCOL_NATIVE_RAW), logctx);
            }
            else
            {
                fprintf(stderr, "EXCEPTION: %s", error.str());
            }
            E->Release();
        }
        catch (IException * E)
        {
            failed = true;
            logctx.CTXLOG("FAILED: %s", sanitizedText.str());
            StringBuffer error;
            E->errorMessage(error);
            logctx.CTXLOG("EXCEPTION: %s", error.str());
            unsigned code = E->errorCode();
            if (QUERYINTERFACE(E, ISEH_Exception))
                code = ROXIE_INTERNAL_ERROR;
            else if (QUERYINTERFACE(E, IOutOfMemException))
                code = ROXIE_MEMORY_ERROR;
            if (client)
            {
                if (isHTTP)
                    client->checkSendHttpException(httpHelper, E, queryName);
                else
                    client->sendException("Roxie", code, error.str(), (protocolFlags & HPCC_PROTOCOL_NATIVE_RAW), logctx);
            }
            else
            {
                fprintf(stderr, "EXCEPTION: %s\n", error.str());
            }
            E->Release();
        }
#ifndef _DEBUG
        catch(...)
        {
            failed = true;
            logctx.CTXLOG("FAILED: %s", sanitizedText.str());
            logctx.CTXLOG("EXCEPTION: Unknown exception");
            {
                if (isHTTP)
                {
                    Owned<IException> E = MakeStringException(ROXIE_INTERNAL_ERROR, "Unknown exception");
                    client->checkSendHttpException(httpHelper, E, queryName);
                }
                else
                    client->sendException("Roxie", ROXIE_INTERNAL_ERROR, "Unknown exception", isBlocked, logctx);
            }
        }
#endif
        if (isHTTP)
        {
            try
            {
                client->flush();
            }
            catch (IException * E)
            {
                StringBuffer error("RoxieSocketWorker failed to write to socket ");
                E->errorMessage(error);
                logctx.CTXLOG("%s", error.str());
                E->Release();

            }
            catch(...)
            {
                logctx.CTXLOG("RoxieSocketWorker failed to write to socket (Unknown exception)");
            }
        }
        unsigned bytesOut = client? client->bytesOut() : 0;
        unsigned elapsed = msTick() - qstart;
        if (continuationNeeded)
        {
            rawText.clear();
            goto readAnother;
        }
        else
        {
            try
            {
                if (client && !isHTTP && !isStatus)
                {
                    if (msgctx->getIntercept())
                    {
                        FlushingStringBuffer response(client, (protocolFlags & HPCC_PROTOCOL_BLOCKED), mlFmt, (protocolFlags & HPCC_PROTOCOL_NATIVE_RAW), false, logctx);
                        response.startDataset("Tracing", NULL, (unsigned) -1);
                        msgctx->outputLogXML(response);
                    }
                    unsigned replyLen = 0;
                    client->write(&replyLen, sizeof(replyLen));
                }
                client.clear();
            }
            catch (IException * E)
            {
                StringBuffer error("RoxieSocketWorker failed to close socket ");
                E->errorMessage(error);
                logctx.CTXLOG("%s", error.str()); // MORE - audience?
                E->Release();

            }
            catch(...)
            {
                logctx.CTXLOG("RoxieSocketWorker failed to close socket (Unknown exception)"); // MORE - audience?
            }
        }
    }
};


IPooledThread *ProtocolSocketListener::createNew()
{
    return new RoxieSocketWorker(this, ep);
}

void ProtocolSocketListener::runOnce(const char *query)
{
    Owned<RoxieSocketWorker> p = new RoxieSocketWorker(this, ep);
    p->runOnce(query);
}

IRoxieListener *createProtocolSocketListener(const char *protocol, IHpccProtocolMsgSink *sink, unsigned port, unsigned poolSize, unsigned listenQueue, bool suspended)
{
    if (traceLevel)
        DBGLOG("Creating Roxie socket listener, protocol %s, pool size %d, listen queue %d%s", protocol, poolSize, listenQueue, suspended?" SUSPENDED":"");
    return new ProtocolSocketListener(sink, port, poolSize, listenQueue, suspended);
}

//================================================================================================================================
