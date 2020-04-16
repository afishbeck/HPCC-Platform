/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2020 HPCC Systems®.

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

#include "espcontext.hpp"
#include "esdl_script.hpp"
#include "wsexcept.hpp"

interface IEsdlTransformOperation : public IInterface
{
    virtual const char *queryMergedTarget() = 0;
    virtual bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) = 0;
    virtual void toDBGLog() = 0;
};

IEsdlTransformOperation *createEsdlTransformOperation(IPropertyTree *element);

inline void esdlOperationError(const char *prefix, const char *error, const char *traceName, bool exception)
{
    StringBuffer s(prefix);
    if (!isEmptyString(traceName))
        s.append(" '").append(traceName).append("' ");
    s.append(error);
    if(exception)
        throw MakeStringException(-1, "%s", s.str());

    IERRLOG("%s", s.str());
}

class CEsdlTransformOperationBase : public CInterfaceOf<IEsdlTransformOperation>
{
protected:
    StringAttr mergedTarget;
public:
    CEsdlTransformOperationBase(IPropertyTree *tree)
    {
        if (tree && tree->hasProp("@_crtTarget"))
            mergedTarget.set(tree->queryProp("@_crtTarget"));
    }

    virtual const char *queryMergedTarget() override
    {
        return mergedTarget;
    }
};

class CEsdlTransformOperationSetValue : public CEsdlTransformOperationBase
{
protected:
    Owned<ICompiledXpath> m_valueXpath;
    StringAttr m_target;
    StringAttr m_traceName;
    bool m_optional = false;
    bool m_append = false;

public:
    CEsdlTransformOperationSetValue(IPropertyTree *tree) : CEsdlTransformOperationBase(tree)
    {
        const char *op = tree->queryName();
        m_append = (op && strieq(op, "xsdl:AppendValue"));

        m_traceName.set(tree->queryProp("@name"));
        m_target.set(tree->queryProp("@target"));
        m_optional = tree->getPropBool("@optional", false);

        if (m_target.isEmpty())
            esdlOperationError("Custom transform: ", "SetValue or AppendValue without target", m_traceName.str(), !m_optional);

        StringAttr strValueXpath(tree->queryProp("@value"));
        if (strValueXpath.length())
            m_valueXpath.setown(compileXpath(strValueXpath));
        else
            esdlOperationError("Custom transform: ", "SetValue or AppendValue without value", m_traceName.str(), !m_optional);
    }

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">%s> SetValue(%s, xpath('%s'))", m_traceName.str(), m_target.str(), m_valueXpath->getXpath());
#endif
    }

    virtual ~CEsdlTransformOperationSetValue(){}

    virtual bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        if (!xpathContext)
            throw MakeStringException(-1, "Could not process custom transform (xpathcontext == null)");
        if (!request)
            return false;
        if (m_target.isEmpty() || !m_valueXpath)
            return false; //only here if "optional" backward compatible support for now (optional syntax errors aren't actually helpful
        try
        {
            StringBuffer value;
            xpathContext->evaluateAsString(m_valueXpath, value);
            return doSet(request, value);
        }
        catch (IException* e)
        {
            StringBuffer msg;
            esdlOperationError("Custom transform: ", e->errorMessage(msg), m_traceName, !m_optional);
            e->Release();
        }
        catch (...)
        {
            esdlOperationError("Custom transform: ", "could not process", m_traceName, !m_optional);
        }
        return false;
    }

    virtual bool doSet(IPropertyTree *tree, const char *value)
    {
        ensurePTree(tree, m_target);
        tree->setProp(m_target, value);
        return true;
    }
};

class CEsdlTransformOperationAppendValue : public CEsdlTransformOperationSetValue
{
public:
    CEsdlTransformOperationAppendValue(IPropertyTree *tree) : CEsdlTransformOperationSetValue(tree){}

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">%s> AppendValue(%s, xpath('%s'))", m_traceName.str(), m_target.str(), m_valueXpath->getXpath());
#endif
    }

    virtual ~CEsdlTransformOperationAppendValue(){}

    virtual bool doSet(IPropertyTree *tree, const char *value) override
    {
        ensurePTree(tree, m_target);
        tree->appendProp(m_target, value);
        return true;
    }
};

class CEsdlTransformOperationAddValue : public CEsdlTransformOperationSetValue
{
public:
    CEsdlTransformOperationAddValue(IPropertyTree *tree) : CEsdlTransformOperationSetValue(tree){}

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">%s> AddValue(%s, xpath('%s'))", m_traceName.str(), m_target.str(), m_valueXpath->getXpath());
#endif
    }

    virtual ~CEsdlTransformOperationAddValue(){}

    virtual bool doSet(IPropertyTree *tree, const char *value) override
    {
        if (tree->getCount(m_target)==0)
        {
            ensurePTree(tree, m_target);
            tree->setProp(m_target, value);
        }
        else
            tree->addProp(m_target, value);
        return true;
    }
};

class CEsdlTransformOperationFail : public CEsdlTransformOperationBase
{
protected:
    StringAttr m_traceName;
    Owned<ICompiledXpath> m_message;
    Owned<ICompiledXpath> m_source;
    Owned<ICompiledXpath> m_code;

public:

    CEsdlTransformOperationFail(IPropertyTree *tree) : CEsdlTransformOperationBase(tree)
    {
        m_traceName.set(tree->queryProp("@name"));
        if (tree->hasProp("@code"))
            m_code.setown(compileXpath(tree->queryProp("@code")));
        if (tree->hasProp("@message"))
            m_message.setown(compileXpath(tree->queryProp("@message")));
    }

    virtual ~CEsdlTransformOperationFail()
    {
    }

    virtual const char *queryOperation(){return "Fail";}

    virtual bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        int code = m_code.get() ? (int) xpathContext->evaluateAsNumber(m_code) : -1;
        StringBuffer msg;
        if (m_message.get())
            xpathContext->evaluateAsString(m_message, msg);
        throw makeStringException(code, msg.str());
        return true; //avoid compilation error
    }

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">%s> Fail with message(%s)", m_traceName.str(), m_message.get() ? m_message->getXpath() : "");
#endif
    }
};

class CEsdlTransformOperationAssert : public CEsdlTransformOperationFail
{
private:
    Owned<ICompiledXpath> m_test; //assert is like a conditional fail

public:

    CEsdlTransformOperationAssert(IPropertyTree *tree) : CEsdlTransformOperationFail(tree)
    {
        if (tree->hasProp("@test"))
            m_test.setown(compileXpath(tree->queryProp("@test")));
    }

    virtual ~CEsdlTransformOperationAssert()
    {
    }

    virtual const char *queryOperation() override {return "Assert";}

    virtual bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        if (m_test && xpathContext->evaluateAsBoolean(m_test))
            return false;
        return CEsdlTransformOperationFail::process(context, request, xpathContext);
    }

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        const char *testXpath = m_test.get() ? m_test->getXpath() : "true()";
        DBGLOG(">%s> Assert if '%s' with message(%s)", m_traceName.str(), testXpath, m_message.get() ? m_message->getXpath() : "");
#endif
    }
};

class CEsdlTransformOperationVariable : public CEsdlTransformOperationBase
{
protected:
    StringAttr m_name;
    Owned<ICompiledXpath> m_select;

public:

    CEsdlTransformOperationVariable(IPropertyTree *tree) : CEsdlTransformOperationBase(tree)
    {
        m_name.set(tree->queryProp("@name"));
        if (tree->hasProp("@select"))
            m_select.setown(compileXpath(tree->queryProp("@select")));
    }

    virtual ~CEsdlTransformOperationVariable()
    {
    }


    virtual bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        return xpathContext->addEvaluateCXVariable(m_name, m_select);
    }

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">%s> Variable with select(%s)", m_name.str(), m_select.get() ? m_select->getXpath() : "");
#endif
    }
};

class CEsdlTransformOperationParameter : public CEsdlTransformOperationBase
{
protected:
    StringAttr m_name;
    Owned<ICompiledXpath> m_select;

public:

    CEsdlTransformOperationParameter(IPropertyTree *tree) : CEsdlTransformOperationBase(tree)
    {
        m_name.set(tree->queryProp("@name"));
        if (tree->hasProp("@select"))
            m_select.setown(compileXpath(tree->queryProp("@select")));
    }

    virtual ~CEsdlTransformOperationParameter()
    {
    }


    virtual bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        return xpathContext->addEvaluateCXParam(m_name.str(), m_select.get());
    }

    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">%s> Parameter with select(%s)", m_name.str(), m_select.get() ? m_select->getXpath() : "");
#endif
    }
};

class CEsdlTransformOperationConditional : public CEsdlTransformOperationBase
{
private:
    Owned<ICompiledXpath> m_test;
    IArrayOf<IEsdlTransformOperation> m_children;
    char m_op = 'i'; //'i'=if, 'w'=when, 'o'=otherwise

public:
    CEsdlTransformOperationConditional(IPropertyTree * tree) : CEsdlTransformOperationBase(tree)
    {
        if (tree)
        {
            const char *op = tree->queryName();
            if (!op || streq(op, "xsdl:if"))
                m_op = 'i';
            else if (streq(op, "xsdl:when"))
                m_op = 'w';
            else if (streq(op, "xsdl:otherwise"))
                m_op = 'o';
            StringBuffer test;
            test.set(tree->queryProp("@test"));
            if (test.length())
                m_test.setown(compileXpath(test.str()));

            loadChildren(tree);
        }
    }

    ~CEsdlTransformOperationConditional(){}

    const char *queryOperator()
    {
        switch(m_op)
        {
        case 'o':
            return "OTHERWISE";
        case 'w':
            return "WHEN";
        default:
            break;
        }
        return "IF";
    }
    bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        if (!evaluate(xpathContext))
            return false;
        ForEachItemIn(i, m_children)
            m_children.item(i).process(context, request, xpathContext);
        return true;
    }

    virtual void toDBGLog () override
    {
    #if defined(_DEBUG)
        DBGLOG (">>>>%s %s ", queryOperator(), m_test ? m_test->getXpath() : "");
        ForEachItemIn(midx, m_children)
            m_children.item(midx).toDBGLog();
        DBGLOG ("<<<<%s<<<<<", queryOperator());
    #endif
    }

private:
    void loadChildren(IPropertyTree * tree)
    {
        Owned<IPropertyTreeIterator> children = tree->getElements("*");
        ForEach(*children)
        {
            Owned<IEsdlTransformOperation> operation = createEsdlTransformOperation(&children->query());
            if (operation)
                m_children.append(*operation.getClear());
        }

    }

    bool evaluate(IXpathContext * xpathContext)
    {
        if (m_op=='o')  //'o'/"otherwise" is unconditional
            return true;
        bool match = false;
        try
        {
            match = xpathContext->evaluateAsBoolean(m_test);
        }
        catch (IException* e)
        {
            StringBuffer msg;
            DBGLOG("%s", e->errorMessage(msg).str());
            e->Release();
        }
        catch (...)
        {
            DBGLOG("CEsdlTransformOperationConditional: Could not evaluate xpath '%s'", m_test.get() ? m_test->getXpath() : "undefined!");
        }
        return match;
    }
};

class CEsdlTransformOperationChoose : public CEsdlTransformOperationBase
{
private:
    IArrayOf<IEsdlTransformOperation> m_conditionals;

public:
    CEsdlTransformOperationChoose(IPropertyTree * tree) : CEsdlTransformOperationBase(tree)
    {
        if (tree)
        {
            loadWhens(tree);
            loadOtherwise(tree);
        }
    }

    ~CEsdlTransformOperationChoose(){}

    bool process(IEspContext * context, IPropertyTree *request, IXpathContext * xpathContext) override
    {
        ForEachItemIn(i, m_conditionals)
        {
            if (m_conditionals.item(i).process(context, request, xpathContext))
                return true;
        }
        return false;
    }

    virtual void toDBGLog () override
    {
    #if defined(_DEBUG)
        DBGLOG (">>>>>>>>>>>CHOOSE>>>>>>>>>>");
        ForEachItemIn(i, m_conditionals)
            m_conditionals.item(i).toDBGLog();
        DBGLOG (">>>>>>>>>>>>CHOOSE>>>>>>>>>>");
    #endif
    }

private:
    void loadWhens(IPropertyTree * tree)
    {
        Owned<IPropertyTreeIterator> children = tree->getElements("xsdl:when");
        ForEach(*children)
            m_conditionals.append(*new CEsdlTransformOperationConditional(&children->query()));
    }

    void loadOtherwise(IPropertyTree * tree)
    {
        IPropertyTree * otherwise = tree->queryPropTree("xsdl:otherwise");
        if (!otherwise)
            return;
        m_conditionals.append(*new CEsdlTransformOperationConditional(otherwise));
    }
};

void processServiceAndMethodTransforms(std::initializer_list<IEsdlCustomTransform *> const &transforms, IEspContext * context, IPropertyTree *tgtcfg, const char *service, const char *method, const char* reqtype, StringBuffer & request, IPropertyTree * bindingCfg)
{
    LogLevel level = LogMax;
    if (!transforms.size())
        return;

    if (request.length()!=0)
    {
        if (level >= LogMax)
        {
            DBGLOG("ORIGINAL REQUEST: %s", request.str());
            StringBuffer marshalled;
            if (bindingCfg)
                toXML(bindingCfg, marshalled.clear());
            DBGLOG("BINDING CONFIG: %s", marshalled.str());
            if (tgtcfg)
                toXML(tgtcfg, marshalled.clear());
            DBGLOG("TARGET CONFIG: %s", marshalled.str());
        }

        Owned<IXpathContext> xpathContext = getXpathContext(request.str());
        registerEsdlXPathExtensions(xpathContext, context);

        VStringBuffer ver("%g", context->getClientVersion());
        if(!xpathContext->addVariable("clientversion", ver.str()))
            OERRLOG("Could not set custom transform variable: clientversion:'%s'", ver.str());

        //in case transform wants to make use of these values:
        xpathContext->addVariable("query", tgtcfg->queryProp("@queryname"));
        xpathContext->addVariable("method", method);
        xpathContext->addVariable("service", service);
        xpathContext->addVariable("request", reqtype);

        ISecUser *user = context->queryUser();
        if (user)
        {
            static const std::map<SecUserStatus, const char*> statusLabels =
            {
#define STATUS_LABEL_NODE(s) { s, #s }
                STATUS_LABEL_NODE(SecUserStatus_Inhouse),
                STATUS_LABEL_NODE(SecUserStatus_Active),
                STATUS_LABEL_NODE(SecUserStatus_Exempt),
                STATUS_LABEL_NODE(SecUserStatus_FreeTrial),
                STATUS_LABEL_NODE(SecUserStatus_csdemo),
                STATUS_LABEL_NODE(SecUserStatus_Rollover),
                STATUS_LABEL_NODE(SecUserStatus_Suspended),
                STATUS_LABEL_NODE(SecUserStatus_Terminated),
                STATUS_LABEL_NODE(SecUserStatus_TrialExpired),
                STATUS_LABEL_NODE(SecUserStatus_Status_Hold),
                STATUS_LABEL_NODE(SecUserStatus_Unknown),
#undef STATUS_LABEL_NODE
            };

            Owned<IPropertyIterator> userPropIt = user->getPropertyIterator();
            ForEach(*userPropIt)
            {
                const char *name = userPropIt->getPropKey();
                if (name && *name)
                    xpathContext->addVariable(name, user->getProperty(name));
            }

            auto it = statusLabels.find(user->getStatus());

            xpathContext->addVariable("espUserName", user->getName());
            xpathContext->addVariable("espUserRealm", user->getRealm() ? user->getRealm() : "");
            xpathContext->addVariable("espUserPeer", user->getPeer() ? user->getPeer() : "");
            xpathContext->addVariable("espUserStatus", VStringBuffer("%d", int(user->getStatus())));
            if (it != statusLabels.end())
                xpathContext->addVariable("espUserStatusString", it->second);
            else
                throw MakeStringException(-1, "encountered unexpected secure user status (%d) while processing transform", int(user->getStatus()));
        }
        else
        {
            // enable transforms to distinguish secure versus insecure requests
            xpathContext->addVariable("espUserName", "");
        }

        Owned<IPropertyTreeIterator> configParams;
        if (bindingCfg)
            configParams.setown(bindingCfg->getElements("Transform/Param"));
        if (configParams)
        {
            ForEach(*configParams)
            {
                IPropertyTree & currentParam = configParams->query();
                if (currentParam.hasProp("@select"))
                    xpathContext->addEvaluateVariable(currentParam.queryProp("@name"), currentParam.queryProp("@select"));
                else
                    xpathContext->addVariable(currentParam.queryProp("@name"), currentParam.queryProp("@value"));
            }
        }
        Owned<IPropertyTree> theroot = createPTreeFromXMLString(request.str());
        StringBuffer defaultTarget;
            //This default gives us backward compatibility with only being able to write to the actual request
        const char *tgtQueryName = tgtcfg->queryProp("@queryname");
        defaultTarget.setf("soap:Body/%s/%s", tgtQueryName ? tgtQueryName : method, reqtype);

        for ( auto&& item : transforms)
        {
            if (item)
                item->processTransform(context, theroot, xpathContext, defaultTarget);
        }

        toXML(theroot, request.clear());

        DBGLOG(1,"MODIFIED REQUEST: %s", request.str());
    }
}

void processServiceAndMethodTransforms(std::initializer_list<IEsdlCustomTransform *> const &transforms, IEspContext * context, IPropertyTree *tgtcfg, IEsdlDefService &srvdef, IEsdlDefMethod &mthdef, StringBuffer & request, IPropertyTree * bindingCfg)
{
    processServiceAndMethodTransforms(transforms, context, tgtcfg, srvdef.queryName(), mthdef.queryMethodName(), mthdef.queryRequestType(), request, bindingCfg);
}

IEsdlTransformOperation *createEsdlTransformOperation(IPropertyTree *element)
{
    const char *op = element->queryName();
    if (strieq(op, "xsdl:choose"))
        return new CEsdlTransformOperationChoose(element);
    if (strieq(op, "xsdl:if"))
        return new CEsdlTransformOperationConditional(element);
    if (streq(op, "xsdl:SetValue"))
        return new CEsdlTransformOperationSetValue(element);
    if (streq(op, "xsdl:AppendValue"))
        return new CEsdlTransformOperationAppendValue(element);
    if (streq(op, "xsdl:AddValue"))
        return new CEsdlTransformOperationAddValue(element);
    if (streq(op, "xsdl:Fail"))
        return new CEsdlTransformOperationFail(element);
    if (streq(op, "xsdl:Assert"))
        return new CEsdlTransformOperationAssert(element);
    return nullptr;
}

static IPropertyTree *getTargetPTree(IPropertyTree *tree, IXpathContext *xpathContext, const char *target)
{
    StringBuffer xpath(target);
    if (xpath.length())
    {
        //we can use real xpath processing in the future, for now simple substitution is fine
        StringBuffer variable;
        xpath.replaceString("{$query}", xpathContext->getVariable("query", variable));
        xpath.replaceString("{$method}", xpathContext->getVariable("method", variable.clear()));
        xpath.replaceString("{$service}", xpathContext->getVariable("service", variable.clear()));
        xpath.replaceString("{$request}", xpathContext->getVariable("request", variable.clear()));

        IPropertyTree *child = tree->queryPropTree(xpath.str());  //get pointer to the write-able area
        if (!child)
            throw MakeStringException(-1, "EsdlCustomTransform error getting target xpath %s", xpath.str());
        return child;
    }
    return tree;
}
static IPropertyTree *getOperationTargetPTree(MapStringToMyClass<IPropertyTree> &treeMap, IPropertyTree *currentTree, IEsdlTransformOperation &operation, IPropertyTree *tree, IXpathContext *xpathContext, const char *target)
{
    const char *mergedTarget = operation.queryMergedTarget();
    if (isEmptyString(mergedTarget) || streq(mergedTarget, target))
        return currentTree;
    IPropertyTree *opTree = treeMap.getValue(mergedTarget);
    if (opTree)
        return opTree;
    opTree = getTargetPTree(tree, xpathContext, mergedTarget);
    if (opTree)
        treeMap.setValue(mergedTarget, LINK(opTree));
    return opTree;
}

class CEsdlCustomTransform : public CInterfaceOf<IEsdlCustomTransform>
{
private:
    IArrayOf<IEsdlTransformOperation> m_variables; //keep separate and only at top level for now
    IArrayOf<IEsdlTransformOperation> m_operations;
    StringAttr m_name;
    StringAttr m_target;

public:
    CEsdlCustomTransform(){}
    CEsdlCustomTransform(IPropertyTree &tree)
    {
        m_name.set(tree.queryProp("@name"));
        m_target.set(tree.queryProp("@target"));

        DBGLOG("Compiling custom ESDL Transform: '%s'", m_name.str());

        Owned<IPropertyTreeIterator> parameters = tree.getElements("xsdl:param");
        ForEach(*parameters)
            m_variables.append(*new CEsdlTransformOperationParameter(&parameters->query()));

        Owned<IPropertyTreeIterator> variables = tree.getElements("xsdl:variable");
        ForEach(*variables)
            m_variables.append(*new CEsdlTransformOperationVariable(&variables->query()));

        Owned<IEsdlTransformOperation> operation;
        Owned<IPropertyTreeIterator> children = tree.getElements("*");
        ForEach(*children)
        {
            operation.setown(createEsdlTransformOperation(&children->query()));
            if (operation)
                m_operations.append(*operation.getClear());
        }
    }


    virtual void toDBGLog() override
    {
#if defined(_DEBUG)
        DBGLOG(">>>>>>>>>>>>>>>>transform: '%s'>>>>>>>>>>", m_name.str());
        ForEachItemIn(i, m_operations)
            m_operations.item(i).toDBGLog();
        DBGLOG("<<<<<<<<<<<<<<<<transform<<<<<<<<<<<<");
#endif
      }

    virtual ~CEsdlCustomTransform(){}

    void processTransform(IEspContext * context, IPropertyTree *theroot, IXpathContext *xpathContext, const char *target) override
    {
        if (m_target.length())
            target = m_target.str();
        MapStringToMyClass<IPropertyTree> treeMap; //cache trees because when there are merged targets they are likely to repeat
        IPropertyTree *txTree = getTargetPTree(theroot, xpathContext, target);
        treeMap.setValue(target, LINK(txTree));
        ForEachItemIn(v, m_variables)
            m_variables.item(v).process(context, txTree, xpathContext);
        ForEachItemIn(i, m_operations)
        {
            IPropertyTree *opTree = getOperationTargetPTree(treeMap, txTree, m_operations.item(i), theroot, xpathContext, target);
            m_operations.item(i).process(context, opTree, xpathContext);
        }
    }

    void processTransform(IEspContext * context, IPropertyTree *tgtcfg, IEsdlDefService &srvdef, IEsdlDefMethod &mthdef, StringBuffer & request, IPropertyTree * bindingCfg) override
    {
        processServiceAndMethodTransforms({static_cast<IEsdlCustomTransform*>(this)}, context, tgtcfg, srvdef, mthdef, request, bindingCfg);
    }

    void processTransform(IEspContext * context, IPropertyTree *tgtcfg, const char *service, const char *method, const char* reqtype, StringBuffer & request, IPropertyTree * bindingCfg) override
    {
        processServiceAndMethodTransforms({static_cast<IEsdlCustomTransform*>(this)}, context, tgtcfg, service, method, reqtype, request, bindingCfg);
    }
};

IEsdlCustomTransform *createEsdlCustomTransform(IPropertyTree &tree)
{
    return new CEsdlCustomTransform(tree);
}
