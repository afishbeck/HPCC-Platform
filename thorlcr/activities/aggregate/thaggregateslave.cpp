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

#include "platform.h"
#include "jlib.hpp"
#include "jiface.hpp"       // IInterface defined in jlib
#include "mpbuff.hpp"
#include "mpcomm.hpp"
#include "mptag.hpp"

#include "eclhelper.hpp"        // for IHThorAggregateArg
#include "slave.ipp"
#include "thbufdef.hpp"

#include "thactivityutil.ipp"
#include "thaggregateslave.ipp"

class AggregateSlaveBase : public CSlaveActivity, public CThorDataLink
{
protected:
    bool hadElement, inputStopped;
    IThorDataLink *input;

    void doStopInput()
    {
        if (inputStopped)
            return;
        inputStopped = true;
        stopInput(input);
    }
    void doStart()
    {
        hadElement = false;
        inputStopped = false;
        input = inputs.item(0);
        startInput(input);
        if (input->isGrouped())
            ActPrintLog("Grouped mismatch");
    }
    const void *getResult(const void *firstRow)
    {
        IHThorAggregateArg *helper = (IHThorAggregateArg *)baseHelper.get();
        unsigned numPartialResults = container.queryJob().querySlaves();
        if (1 == numPartialResults)
            return firstRow;

        CThorExpandingRowArray partialResults(*this, this, true, stableSort_none, true, numPartialResults);
        if (hadElement)
            partialResults.setRow(0, firstRow);
        --numPartialResults;

        size32_t sz;
        while (numPartialResults--)
        {
            CMessageBuffer msg;
            rank_t sender;
            if (!receiveMsg(msg, RANK_ALL, mpTag, &sender))
                return NULL;
            if (abortSoon)
                return NULL;
            msg.read(sz);
            if (sz)
            {
                assertex(NULL == partialResults.query(sender-1));
                CThorStreamDeserializerSource mds(sz, msg.readDirect(sz));
                RtlDynamicRowBuilder rowBuilder(queryRowAllocator());
                size32_t sz = queryRowDeserializer()->deserialize(rowBuilder, mds);
                partialResults.setRow(sender-1, rowBuilder.finalizeRowClear(sz));
            }
        }
        RtlDynamicRowBuilder rowBuilder(queryRowAllocator(), false);
        bool first = true;
        numPartialResults = container.queryJob().querySlaves();
        unsigned p=0;
        for (;p<numPartialResults; p++)
        {
            const void *row = partialResults.query(p);
            if (row)
            {
                if (first)
                {
                    first = false;
                    sz = cloneRow(rowBuilder, row, queryRowMetaData());
                }
                else
                    sz = helper->mergeAggregate(rowBuilder, row);
            }
        }
        if (first)
            sz = helper->clearAggregate(rowBuilder);
        return rowBuilder.finalizeRowClear(sz);
    }
    void sendResult(const void *row, IOutputRowSerializer *serializer, rank_t dst)
    {
        CMessageBuffer mb;
        DelayedSizeMarker sizeMark(mb);
        if (row&&hadElement) {
            CMemoryRowSerializer mbs(mb);
            serializer->serialize(mbs,(const byte *)row);
            sizeMark.write();
        }
        container.queryJob().queryJobComm().send(mb, dst, mpTag);
    }
public:
    AggregateSlaveBase(CGraphElementBase *_container)
        : CSlaveActivity(_container), CThorDataLink(this)
    {
        input = NULL;
        hadElement = inputStopped = false;
    }
    virtual void init(MemoryBuffer &data, MemoryBuffer &slaveData)
    {
        if (!container.queryLocal())
            mpTag = container.queryJob().deserializeMPTag(data);
        appendOutputLinked(this);
    }
    virtual bool isGrouped() { return false; }
};

//

class AggregateSlaveActivity : public AggregateSlaveBase
{
    bool eof;
    IHThorAggregateArg * helper;

public:
    IMPLEMENT_IINTERFACE_USING(CSimpleInterface);

    AggregateSlaveActivity(CGraphElementBase *container) : AggregateSlaveBase(container)
    {
        helper = (IHThorAggregateArg *)queryHelper();
        eof = false;
    }
    virtual void abort()
    {
        AggregateSlaveBase::abort();
        if (firstNode())
            cancelReceiveMsg(1, mpTag);
    }
    virtual void start()
    {
        ActivityTimer s(totalCycles, timeActivities, NULL);
        doStart();
        eof = false;
        dataLinkStart("AGGREGATE", container.queryId());
    }
    virtual void stop()
    {
        doStopInput();
        dataLinkStop();
    }
    CATCH_NEXTROW()
    {
        ActivityTimer t(totalCycles, timeActivities, NULL);
        if (abortSoon || eof)
            return NULL;
        eof = true;

        OwnedConstThorRow next = input->ungroupedNextRow();
        RtlDynamicRowBuilder resultcr(queryRowAllocator());
        size32_t sz = helper->clearAggregate(resultcr);         
        if (next)
        {
            hadElement = true;
            sz = helper->processFirst(resultcr, next);
            if (container.getKind() != TAKexistsaggregate)
            {
                while (!abortSoon)
                {
                    next.setown(input->ungroupedNextRow());
                    if (!next)
                        break;
                    sz = helper->processNext(resultcr, next);
                }
            }
        }
        doStopInput();
        if (!firstNode())
        {
            OwnedConstThorRow result(resultcr.finalizeRowClear(sz));
            sendResult(result.get(),queryRowSerializer(), 1); // send partial result
            return NULL;
        }
        OwnedConstThorRow ret = getResult(resultcr.finalizeRowClear(sz));
        if (ret)
        {
            dataLinkIncrement();
            return ret.getClear();
        }
        sz = helper->clearAggregate(resultcr);  
        return resultcr.finalizeRowClear(sz);
    }
    virtual void getMetaInfo(ThorDataLinkMetaInfo &info)
    {
        initMetaInfo(info);
        info.singleRowOutput = true;
        info.totalRowsMin=1;
        info.totalRowsMax=1;
    }
};

//

class ThroughAggregateSlaveActivity : public AggregateSlaveBase
{
    IHThorThroughAggregateArg *helper;
    RtlDynamicRowBuilder partResult;
    size32_t partResultSize;
    Owned<IRowInterfaces> aggrowif;

    void doStopInput()
    {
        OwnedConstThorRow partrow = partResult.finalizeRowClear(partResultSize);
        if (!firstNode())
            sendResult(partrow.get(), aggrowif->queryRowSerializer(), 1);
        else
        {
            OwnedConstThorRow ret = getResult(partrow.getClear());
            sendResult(ret, aggrowif->queryRowSerializer(), 0); // send to master
        }
        AggregateSlaveBase::doStopInput();
        dataLinkStop();
    }
    void readRest()
    {
        loop
        {
            OwnedConstThorRow row = ungroupedNextRow();
            if (!row)
                break;
        }
    }
    void process(const void *r)
    {
        if (hadElement)
            partResultSize = helper->processNext(partResult, r);
        else
        {
            partResultSize = helper->processFirst(partResult, r);
            hadElement = true;
        }
    }
public:
    IMPLEMENT_IINTERFACE_USING(CSimpleInterface);

    ThroughAggregateSlaveActivity(CGraphElementBase *container) : AggregateSlaveBase(container), partResult(NULL)
    {
        helper = (IHThorThroughAggregateArg *)queryHelper();
        partResultSize = 0;
    }
    virtual void start()
    {
        ActivityTimer s(totalCycles, timeActivities, NULL);
        doStart();
        aggrowif.setown(createRowInterfaces(helper->queryAggregateRecordSize(),queryActivityId(),queryCodeContext()));
        partResult.setAllocator(aggrowif->queryRowAllocator()).ensureRow();
        helper->clearAggregate(partResult);
        dataLinkStart("THROUGHAGGREGATE", container.queryId());
    }
    virtual void stop()
    {
        if (inputStopped) 
            return;
        readRest();
        doStopInput();
        //GH: Shouldn't there be something like the following - in all activities with a member, otherwise the allocator may have gone??
        partResult.clear();
    }
    CATCH_NEXTROW()
    {
        ActivityTimer t(totalCycles, timeActivities, NULL);
        if (inputStopped)
            return NULL;
        OwnedConstThorRow row = input->ungroupedNextRow();
        if (!row)
            return NULL;
        process(row);
        dataLinkIncrement();
        return row.getClear();
    }
    virtual void getMetaInfo(ThorDataLinkMetaInfo &info)
    {
        inputs.item(0)->getMetaInfo(info);
    }
};


CActivityBase *createAggregateSlave(CGraphElementBase *container)
{
    //NB: only used if global, createGroupAggregateSlave used if child,local or grouped
    return new AggregateSlaveActivity(container);
}

CActivityBase *createThroughAggregateSlave(CGraphElementBase *container)
{
    if (container->queryOwner().queryOwner() && !container->queryOwner().isGlobal())
        throwUnexpected();
    return new ThroughAggregateSlaveActivity(container);
}

