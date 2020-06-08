/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2018 HPCC Systems®.

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

#include "jstring.hpp"
#include "jdebug.hpp"
#include "jptree.hpp"
#include "jexcept.hpp"
#include "jlog.hpp"

#include <libxml/xmlmemory.h>
#include <libxml/parserInternals.h>
#include <libxml/debugXML.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlIO.h>
#include <libxml/xinclude.h>
#include <libxml/catalog.h>
#include <libxml/xpathInternals.h>
#include <libxml/xpath.h>
#include <libxml/xmlschemas.h>
#include <libxml/hash.h>
#include <libexslt/exslt.h>

#include "xpathprocessor.hpp"
#include "xmlerror.hpp"

#include <map>
#include <stack>
#include <string>
#include <memory>

static inline char *skipWS(char *s)
{
    while (isspace(*s)) s++;
    return s;
}
static char *markEndGetNext(char *line)
{
    char *end = (char *)strchr(line, '\n');
    if (!end)
        return nullptr;
    *end=0;
    if (isEmptyString(++end))
        return nullptr;
    return end;
}
static char *extractFromLineGetNext(StringArray &functions, StringArray &variables, char *line)
{
    line = skipWS(line);
    if (isEmptyString(line))
        return nullptr;
    char *next = markEndGetNext(line);
    if (strncmp(line, "FUNCTION", 8)==0)
    {
        char *paren = (char *)strchr(line, '(');
        if (paren)
            *paren=0;
        functions.append(skipWS(line+8+1));
    }
    else if (strncmp(line, "VARIABLE", 8)==0)
    {
        variables.append(skipWS(line+8+1));
    }
    return next;
}

class CLibCompiledXpath : public CInterfaceOf<ICompiledXpath>
{
private:
    xmlXPathCompExprPtr m_compiledXpathExpression = nullptr;
    StringBuffer m_xpath;
    ReadWriteLock m_rwlock;

public:
    CLibCompiledXpath(const char * xpath)
    {
        m_xpath.set(xpath);
        m_compiledXpathExpression = xmlXPathCompile(BAD_CAST m_xpath.str());
    }
    ~CLibCompiledXpath()
    {
        xmlXPathFreeCompExpr(m_compiledXpathExpression);
    }
    const char * getXpath()
    {
        return m_xpath.str();
    }
    xmlXPathCompExprPtr getCompiledXPathExpression()
    {
        return m_compiledXpathExpression;
    }

    virtual void extractReferences(StringArray &functions, StringArray &variables) override
    {
        char *buf = nullptr;
        size_t len = 0;

        FILE *stream = open_memstream(&buf, &len);
        if (stream == nullptr)
            return;

        xmlXPathDebugDumpCompExpr(stream, m_compiledXpathExpression, 0);
        fputc(0, stream);
        fflush(stream);
        fclose (stream);
        char *line = buf;
        while (line)
            line = extractFromLineGetNext(functions, variables, line);
        free (buf);
    }
};
static xmlXPathObjectPtr variableLookupFunc(void *data, const xmlChar *name, const xmlChar *ns_uri);

typedef std::map<std::string, xmlXPathObjectPtr> XPathObjectMap;

class CLibXpathScope
{
public:
    StringAttr name; //in future allow named parent access?
    XPathObjectMap variables;

public:
    CLibXpathScope(const char *_name) : name(_name){}
    ~CLibXpathScope()
    {
        for (XPathObjectMap::iterator it=variables.begin(); it!=variables.end(); ++it)
            xmlXPathFreeObject(it->second);
    }
    bool setObject(const char *key, xmlXPathObjectPtr obj)
    {
        std::pair<XPathObjectMap::iterator,bool> ret = variables.emplace(key, obj);
        if (ret.second==true)
            return true;
        //within scope, behave exactly like xmlXPathContext variables are added now, which seems to be that they are replaced
        //if we're preventing replacing variables we need to handle elsewhere
        //  and still replace external values when treated as xsdl:variables, but not when treated as xsdl:params
        if (ret.first->second)
            xmlXPathFreeObject(ret.first->second);
        ret.first->second = obj;
        return true;
    }
    xmlXPathObjectPtr getObject(const char *key)
    {
        XPathObjectMap::iterator it = variables.find(key);
        if (it == variables.end())
            return nullptr;
        return it->second;
    }
};

typedef std::vector<std::unique_ptr<CLibXpathScope>> XPathScopeVector;
typedef std::map<std::string, ICompiledXpath*> XPathInputMap;

class XpathContextState
{
private:
    xmlDocPtr doc = nullptr;
    xmlNodePtr node = nullptr;
    int contextSize = 0;
    int proximityPosition = 0;
public:
    XpathContextState(xmlXPathContextPtr ctx)
    {
        doc = ctx->doc;
        node = ctx->node;
        contextSize = ctx->contextSize;
        proximityPosition = ctx->proximityPosition;
    }

    void restore(xmlXPathContextPtr ctx)
    {
        ctx->doc = doc;
        ctx->node = node;
        ctx->contextSize = contextSize;
        ctx->proximityPosition = proximityPosition;
    }
};

typedef std::vector<XpathContextState> XPathContextStateVector;

class CLibXpathContext : public CInterfaceOf<IXpathContext>
{
public:
    XPathInputMap provided;
    xmlDocPtr m_xmlDoc = nullptr;
    xmlXPathContextPtr m_xpathContext = nullptr;
    ReadWriteLock m_rwlock;
    XPathScopeVector scopes;
    bool strictParameterDeclaration = true;
    bool ownedDoc = false;

    //saved state
    XPathContextStateVector saved;

public:
    CLibXpathContext(const char * xmldoc, bool _strictParameterDeclaration) : strictParameterDeclaration(_strictParameterDeclaration)
    {
        beginScope("/");
        setXmlDoc(xmldoc);
    }

    CLibXpathContext(xmlDocPtr doc, xmlNodePtr node, bool _strictParameterDeclaration) : strictParameterDeclaration(_strictParameterDeclaration)
    {
        beginScope("/");
        setContextDocument(doc, node);
    }

    ~CLibXpathContext()
    {
        for (XPathInputMap::iterator it=provided.begin(); it!=provided.end(); ++it)
            it->second->Release();
        xmlXPathFreeContext(m_xpathContext);
        if (ownedDoc)
            xmlFreeDoc(m_xmlDoc);
    }

    void registerExslt()
    {
        exsltDateXpathCtxtRegister(m_xpathContext, (xmlChar*)"date");
        exsltMathXpathCtxtRegister(m_xpathContext, (xmlChar*)"math");
        exsltSetsXpathCtxtRegister(m_xpathContext, (xmlChar*)"set");
        exsltStrXpathCtxtRegister(m_xpathContext, (xmlChar*)"str");
    }
    void pushLocation()
    {
        WriteLockBlock wblock(m_rwlock);
        saved.emplace_back(XpathContextState(m_xpathContext));
    }

    void setLocation(xmlDocPtr doc, xmlNodePtr node, int contextSize, int proximityPosition)
    {
        WriteLockBlock wblock(m_rwlock);
        m_xpathContext->doc = doc;
        m_xpathContext->node = node;
        m_xpathContext->contextSize = contextSize;
        m_xpathContext->proximityPosition = proximityPosition;
    }

    void setLocation(xmlXPathContextPtr ctx)
    {
        WriteLockBlock wblock(m_rwlock);
        m_xpathContext->doc = ctx->doc;
        m_xpathContext->node = ctx->node;
        m_xpathContext->contextSize = ctx->contextSize;
        m_xpathContext->proximityPosition = ctx->proximityPosition;
    }

    void popLocation()
    {
        WriteLockBlock wblock(m_rwlock);
        saved.back().restore(m_xpathContext);
        saved.pop_back();
    }

    void beginScope(const char *name) override
    {
        WriteLockBlock wblock(m_rwlock);
        scopes.emplace_back(new CLibXpathScope(name));
    }

    void endScope() override
    {
        WriteLockBlock wblock(m_rwlock);
        if (scopes.size()>1) //preserve root scope
            scopes.pop_back();
    }

    static void tableScanCallback(void *payload, void *data, xmlChar *name)
    {
        DBGLOG("k/v == [%s,%s]\n", (char *) name, (char *) payload);
    }

    virtual void registerNamespace(const char *prefix, const char * uri) override
    {
        if (m_xpathContext)
        {
            WriteLockBlock wblock(m_rwlock);
            xmlXPathRegisterNs(m_xpathContext, (const xmlChar *) prefix, (const xmlChar *) uri);
        }
    }

    virtual void registerFunction(const char *xmlns, const char * name, void *f) override
    {
        if (m_xpathContext)
        {
            WriteLockBlock wblock(m_rwlock);
            xmlXPathRegisterFuncNS(m_xpathContext, (const xmlChar *) name, (const xmlChar *) xmlns, (xmlXPathFunction) f);
        }
    }

    virtual void setUserData(void *userdata) override
    {
        if (m_xpathContext)
        {
            WriteLockBlock wblock(m_rwlock);
            m_xpathContext->userData = userdata;
        }
    }

    virtual void *getUserData() override
    {
        if (m_xpathContext)
        {
            ReadLockBlock wblock(m_rwlock);
            return m_xpathContext->userData;
        }
        return nullptr;
    }

    inline CLibXpathScope *getCurrentScope()
    {
        ReadLockBlock wblock(m_rwlock);
        assertex(scopes.size());
        return scopes.back().get();
    }
    xmlXPathObjectPtr findVariable(const char *name, const char *ns_uri, CLibXpathScope *scope)
    {
        const char *fullname = name;
        StringBuffer s;
        if (!isEmptyString(ns_uri))
            fullname = s.append(ns_uri).append(':').append(name).str();

        ReadLockBlock wblock(m_rwlock);
        xmlXPathObjectPtr obj = nullptr;
        if (scope)
            return scope->getObject(fullname);

        for (XPathScopeVector::const_reverse_iterator it=scopes.crbegin(); !obj && it!=scopes.crend(); ++it)
            obj = it->get()->getObject(fullname);

        //check libxml2 level variables, shouldn't happen currently but we may want to wrap existing xpathcontexts in the future
        if (!obj)
            obj = (xmlXPathObjectPtr)xmlHashLookup2(m_xpathContext->varHash, (const xmlChar *)name, (const xmlChar *)ns_uri);
        return obj;
    }

    xmlXPathObjectPtr getVariableObject(const char *name, const char *ns_uri, CLibXpathScope *scope)
    {
        return xmlXPathObjectCopy(findVariable(name, ns_uri, scope));
    }

    bool hasVariable(const char *name, const char *ns_uri, CLibXpathScope *scope)
    {
        return (findVariable(name, ns_uri, scope)!=nullptr);
    }

    virtual bool addObjectVariable(const char * name, xmlXPathObjectPtr obj, CLibXpathScope *scope)
    {
        if (isEmptyString(name))
            return false;
        if (m_xpathContext)
        {
            if (!obj)
                throw MakeStringException(-1, "addObjectVariable %s error", name);
            WriteLockBlock wblock(m_rwlock);
            if (!scope && !scopes.empty())
                scope = scopes.back().get();
            if (scope)
                return scope->setObject(name, obj);
            return xmlXPathRegisterVariable(m_xpathContext, (xmlChar *)name, obj) == 0;
        }
        return false;
    }

    bool addStringVariable(const char * name,  const char * val, CLibXpathScope *scope)
    {
        if (!val)
            return false;
        return addObjectVariable(name, xmlXPathNewCString(val), scope);
    }

    virtual bool addXpathVariable(const char * name, const char * xpath, CLibXpathScope *scope)
    {
        if (isEmptyString(xpath))
            addVariable(name, "");
        if (m_xpathContext)
        {
            xmlXPathObjectPtr obj = evaluate(xpath);
            if (!obj)
                throw MakeStringException(-1, "addXpathVariable xpath error %s", xpath);
            return addObjectVariable(name, obj, scope);
        }
        return false;
    }

    bool addCompiledVariable(const char * name, ICompiledXpath * compiled, CLibXpathScope *scope)
    {
        if (!compiled)
            addVariable(name, "");
        if (m_xpathContext)
        {
            CLibCompiledXpath * clibCompiledXpath = static_cast<CLibCompiledXpath *>(compiled);
            xmlXPathObjectPtr obj = evaluate(clibCompiledXpath->getCompiledXPathExpression(), clibCompiledXpath->getXpath());
            if (!obj)
                throw MakeStringException(-1, "addEvaluateVariable xpath error %s", clibCompiledXpath->getXpath());
            return addObjectVariable(name, obj, scope);
        }

        return false;
    }

    virtual bool addInputValue(const char * name, const char * value) override
    {
        if (isEmptyString(name)||isEmptyString(value))
            return false;
        VStringBuffer xpath("'%s'", value);
        return addInputXpath(name, xpath);
    }
    virtual bool addInputXpath(const char * name, const char * xpath) override
    {
        if (isEmptyString(name)||isEmptyString(xpath))
            return false;
        Owned<ICompiledXpath> compiled = compileXpath(xpath);
        if (compiled)
        {
            WriteLockBlock wblock(m_rwlock);
            provided.emplace(name, compiled.getClear());
            return true;
        }
        return false;
    }

    inline ICompiledXpath *findInput(const char *name)
    {
        ReadLockBlock rblock(m_rwlock);
        XPathInputMap::iterator it = provided.find(name);
        if (it == provided.end())
            return nullptr;
         return it->second;
    }

    virtual bool declareCompiledParameter(const char * name, ICompiledXpath * compiled) override
    {
        if (hasVariable(name, nullptr, getCurrentScope()))
            return false;

        //use input value
        ICompiledXpath *inputxp = findInput(name);
        if (inputxp)
            return addCompiledVariable(name, inputxp, getCurrentScope());

        //use default provided
        return addCompiledVariable(name, compiled, getCurrentScope());
    }

    virtual void declareRemainingInputs() override
    {
        for (XPathInputMap::iterator it=provided.begin(); it!=provided.end(); ++it)
            declareCompiledParameter(it->first.c_str(), it->second);
    }

    virtual bool declareParameter(const char * name, const char *value) override
    {
        if (hasVariable(name, nullptr, getCurrentScope()))
            return false;

        //use input value
        ICompiledXpath *input = findInput(name);
        if (input)
            return addCompiledVariable(name, input, getCurrentScope());

        //use default provided
        return addStringVariable(name, value, getCurrentScope());
    }

    virtual bool addXpathVariable(const char * name, const char * xpath) override
    {
        return addXpathVariable(name, xpath, nullptr);
    }


    virtual bool addVariable(const char * name,  const char * val) override
    {
        return addStringVariable(name, val, nullptr);
    }

    virtual bool addCompiledVariable(const char * name, ICompiledXpath * compiled) override
    {
        return addCompiledVariable(name, compiled, nullptr);
    }

    virtual const char * getVariable(const char * name, StringBuffer & variable) override
    {
        if (m_xpathContext)
        {
            ReadLockBlock rblock(m_rwlock);
            xmlXPathObjectPtr ptr = xmlXPathVariableLookupNS(m_xpathContext, (const xmlChar *)name, nullptr);
            if (!ptr)
                return nullptr;
            variable.append((const char *) ptr->stringval);
            xmlXPathFreeObject(ptr);
            return variable;
        }
        return nullptr;
    }
    virtual  IXpathContextIterator *evaluateAsNodeSet(ICompiledXpath * compiled) override
    {
        CLibCompiledXpath * clCompiled = static_cast<CLibCompiledXpath *>(compiled);
        if (!clCompiled)
            throw MakeStringException(XPATHERR_MissingInput,"XpathProcessor:evaluateAsNodeSet: Error: Could not evaluate XPATH");
        return evaluateAsNodeSet(evaluate(clCompiled->getCompiledXPathExpression(), compiled->getXpath()), compiled->getXpath());
    }

    IXpathContextIterator *evaluateAsNodeSet(xmlXPathObjectPtr evaluated, const char* xpath);

    virtual bool evaluateAsBoolean(const char * xpath) override
    {
        if (!xpath || !*xpath)
            throw MakeStringException(XPATHERR_MissingInput,"XpathProcessor:evaluateAsBoolean: Error: Could not evaluate empty XPATH");
        return evaluateAsBoolean(evaluate(xpath), xpath);
    }

    virtual bool evaluateAsString(const char * xpath, StringBuffer & evaluated) override
    {
        if (!xpath || !*xpath)
            throw MakeStringException(XPATHERR_MissingInput,"XpathProcessor:evaluateAsString: Error: Could not evaluate empty XPATH");
        return evaluateAsString(evaluate(xpath), evaluated, xpath);
    }

    virtual bool evaluateAsBoolean(ICompiledXpath * compiledXpath) override
    {
        CLibCompiledXpath * clibCompiledXpath = static_cast<CLibCompiledXpath *>(compiledXpath);
        if (!clibCompiledXpath)
            throw MakeStringException(XPATHERR_MissingInput,"XpathProcessor:evaluateAsBoolean: Error: Missing compiled XPATH");
        return evaluateAsBoolean(evaluate(clibCompiledXpath->getCompiledXPathExpression(), compiledXpath->getXpath()), compiledXpath->getXpath());
    }

    virtual const char * evaluateAsString(ICompiledXpath * compiledXpath, StringBuffer & evaluated) override
    {
        CLibCompiledXpath * clibCompiledXpath = static_cast<CLibCompiledXpath *>(compiledXpath);
        if (!clibCompiledXpath)
            throw MakeStringException(XPATHERR_MissingInput,"XpathProcessor:evaluateAsString: Error: Missing compiled XPATH");
        return evaluateAsString(evaluate(clibCompiledXpath->getCompiledXPathExpression(), compiledXpath->getXpath()), evaluated, compiledXpath->getXpath());
    }

    virtual double evaluateAsNumber(ICompiledXpath * compiledXpath) override
    {
        CLibCompiledXpath * clibCompiledXpath = static_cast<CLibCompiledXpath *>(compiledXpath);
        if (!clibCompiledXpath)
            throw MakeStringException(XPATHERR_MissingInput,"XpathProcessor:evaluateAsNumber: Error: Missing compiled XPATH");
        return evaluateAsNumber(evaluate(clibCompiledXpath->getCompiledXPathExpression(), compiledXpath->getXpath()), compiledXpath->getXpath());
    }

    virtual bool setXmlDoc(const char * xmldoc) override
    {
        if (isEmptyString(xmldoc))
            return false;
        xmlDocPtr doc = xmlParseDoc((const unsigned char *)xmldoc);
        if (doc == nullptr)
        {
            ERRLOG("XpathProcessor:setXmlDoc Error: Unable to parse XMLLib document");
            return false;
        }
        ownedDoc = true;
        return setContextDocument(doc, xmlDocGetRootElement(doc));
    }

private:
    bool setContextDocument(xmlDocPtr doc, xmlNodePtr node)
    {
        WriteLockBlock rblock(m_rwlock);

        m_xmlDoc = doc;
        m_xpathContext = xmlXPathNewContext(m_xmlDoc);
        if(m_xpathContext == nullptr)
        {
            ERRLOG("XpathProcessor:setContextDocument: Error: Unable to create new XMLLib XPath context");
            return false;
        }

        //relative paths need something to be relative to
        if (node)
            m_xpathContext->node = node;
        xmlXPathRegisterVariableLookup(m_xpathContext, variableLookupFunc, this);
        return true;
    }
    bool evaluateAsBoolean(xmlXPathObjectPtr evaluatedXpathObj, const char* xpath)
    {
        if (!evaluatedXpathObj)
        {
            throw MakeStringException(XPATHERR_InvalidInput, "XpathProcessor:evaluateAsBoolean: Error: Could not evaluate XPATH '%s'", xpath);
        }
        if (XPATH_BOOLEAN != evaluatedXpathObj->type)
        {
            xmlXPathFreeObject(evaluatedXpathObj);
            throw MakeStringException(XPATHERR_UnexpectedInput, "XpathProcessor:evaluateAsBoolean: Error: Could not evaluate XPATH '%s' as Boolean", xpath);
        }

        bool bresult = evaluatedXpathObj->boolval;

        xmlXPathFreeObject(evaluatedXpathObj);
        return bresult;
    }

    const char* evaluateAsString(xmlXPathObjectPtr evaluatedXpathObj, StringBuffer& evaluated, const char* xpath)
    {
        if (!evaluatedXpathObj)
            throw MakeStringException(XPATHERR_InvalidInput,"XpathProcessor:evaluateAsString: Error: Could not evaluate XPATH '%s'", xpath);

        evaluated.clear();
        switch (evaluatedXpathObj->type)
        {
            case XPATH_NODESET:
            {
                xmlNodeSetPtr nodes = evaluatedXpathObj->nodesetval;
                for (int i = 0; i < nodes->nodeNr; i++)
                {
                    xmlNodePtr nodeTab = nodes->nodeTab[i];
                    auto nodeContent = xmlNodeGetContent(nodeTab);
                    evaluated.append((const char *)nodeContent);
                    xmlFree(nodeContent);
                }
                break;
            }
            case XPATH_BOOLEAN:
            case XPATH_NUMBER:
            case XPATH_STRING:
            case XPATH_POINT:
            case XPATH_RANGE:
            case XPATH_LOCATIONSET:
            case XPATH_USERS:
            case XPATH_XSLT_TREE:
            {
                evaluatedXpathObj = xmlXPathConvertString (evaluatedXpathObj); //existing object is freed
                if (!evaluatedXpathObj)
                    throw MakeStringException(XPATHERR_UnexpectedInput,"XpathProcessor:evaluateAsString: Error: Could not evaluate XPATH '%s'; could not convert result to string", xpath);
                evaluated.append(evaluatedXpathObj->stringval);
                break;
            }
            default:
            {
                xmlXPathFreeObject(evaluatedXpathObj);
                throw MakeStringException(XPATHERR_UnexpectedInput,"XpathProcessor:evaluateAsString: Error: Could not evaluate XPATH '%s' as string; unexpected type %d", xpath, evaluatedXpathObj->type);
                break;
            }
        }
        xmlXPathFreeObject(evaluatedXpathObj);
        return evaluated.str();
    }

    double evaluateAsNumber(xmlXPathObjectPtr evaluatedXpathObj, const char* xpath)
    {
        if (!evaluatedXpathObj)
            throw MakeStringException(XPATHERR_InvalidInput,"XpathProcessor:evaluateAsNumber: Error: Could not evaluate XPATH '%s'", xpath);
        double ret = xmlXPathCastToNumber(evaluatedXpathObj);
        xmlXPathFreeObject(evaluatedXpathObj);
        return ret;
    }

    virtual xmlXPathObjectPtr evaluate(xmlXPathCompExprPtr compiled, const char *xpath)
    {
        xmlXPathObjectPtr evaluatedXpathObj = nullptr;
        if (compiled)
        {
            ReadLockBlock rlock(m_rwlock);
            if ( m_xpathContext)
            {
                evaluatedXpathObj = xmlXPathCompiledEval(compiled, m_xpathContext);
            }
            else
            {
                throw MakeStringException(XPATHERR_InvalidState,"XpathProcessor:evaluate: Error: Could not evaluate XPATH '%s'", xpath);
            }
        }

        return evaluatedXpathObj;
    }

    virtual xmlXPathObjectPtr evaluate(const char * xpath)
    {
        xmlXPathObjectPtr evaluatedXpathObj = nullptr;
        if (xpath && *xpath)
        {
            ReadLockBlock rlock(m_rwlock);
            if ( m_xpathContext)
            {
                evaluatedXpathObj = xmlXPathEval((const xmlChar *)xpath, m_xpathContext);
            }
            else
            {
                throw MakeStringException(XPATHERR_InvalidState,"XpathProcessor:evaluate: Error: Could not evaluate XPATH '%s'; ensure xmldoc has been set", xpath);
            }
        }

        return evaluatedXpathObj;
    }
};

class XPathNodeSetIterator : public CInterfaceOf<IXpathContextIterator>
{
public:
    CLibXpathContext *context;
    xmlNodeSetPtr list;
    unsigned pos = 0;

public:
    XPathNodeSetIterator(CLibXpathContext *xpctx, xmlNodeSetPtr nodeset) : context(xpctx), list(nodeset)
    {
        context->pushLocation();
        context->m_xpathContext->contextSize = xmlXPathNodeSetGetLength(list);
    }

    virtual ~XPathNodeSetIterator()
    {
        context->popLocation();
        if (list)
            xmlXPathFreeNodeSet(list);
    }

    bool update(int newpos)
    {
        pos = newpos;
        if (!isValid())
            return false;
        xmlNodePtr node = xmlXPathNodeSetItem(list, pos);
        context->m_xpathContext->node = node;
        if ((node->type != XML_NAMESPACE_DECL && node->doc != nullptr))
            context->m_xpathContext->doc = node->doc;
        context->m_xpathContext->proximityPosition = pos + 1;
        return true;
    }
    virtual bool first()
    {
        if (xmlXPathNodeSetGetLength(list)==0)
            return false;
        return update(0);
    }

    virtual bool next()
    {
        return update(pos+1);
    }
    virtual bool isValid()
    {
        return (pos < xmlXPathNodeSetGetLength(list));
    }
    virtual IXpathContext  & query()
    {
        return *context;
    }
};

IXpathContextIterator *CLibXpathContext::evaluateAsNodeSet(xmlXPathObjectPtr evaluated, const char* xpath)
{
    if (!evaluated)
    {
        throw MakeStringException(XPATHERR_InvalidInput, "XpathProcessor:evaluateAsNodeSet: Error: Could not evaluate XPATH '%s'", xpath);
    }

    if (XPATH_NODESET != evaluated->type)
    {
        xmlXPathFreeObject(evaluated);
        throw MakeStringException(XPATHERR_UnexpectedInput, "XpathProcessor:evaluateAsNodeSet: Error: Could not evaluate XPATH '%s' as NodeSet", xpath);
    }


    xmlNodeSetPtr ns = evaluated->nodesetval;
    evaluated->nodesetval = nullptr;
    xmlXPathFreeObject(evaluated);

    return new XPathNodeSetIterator(this, ns);
}

static xmlXPathObjectPtr variableLookupFunc(void *data, const xmlChar *name, const xmlChar *ns_uri)
{
    CLibXpathContext *ctxt = (CLibXpathContext *) data;
    if (!ctxt)
        return nullptr;
    return ctxt->getVariableObject((const char *)name, (const char *)ns_uri, nullptr);
}

extern ICompiledXpath* compileXpath(const char * xpath)
{
    return new CLibCompiledXpath(xpath);
}

typedef std::vector<xmlNsPtr> xmlNsStack;

xmlNsPtr lookupXmlNs(xmlNsStack &nss, const char *prefix)
{
    //search in revers order.  Allows overloading values on stack
    for(xmlNsStack::reverse_iterator it = nss.rbegin(); it != nss.rend(); ++it)
    {
        if (streq((const char *)(*it)->prefix, prefix))
            return *it;
    }
    return nullptr;
}
xmlNsPtr createNsFromXmlns(xmlNodePtr node, const char *attname, const char *uri, xmlNsPtr ns)
{
    if (*attname=='@')
        attname++;
    if (strncmp(attname, "xmlns", 5)==0)
    {
        attname+=5;
        if (*attname==':')
            attname++;
    }
    if (ns && streq((const char *)ns->href, uri)) //preprocessed the current prefix
        return nullptr;
    return xmlNewNs(node, (const xmlChar *)uri, (const xmlChar *)attname);
}
void addChildFromPtree(xmlNodePtr parent, IPropertyTree &tree, const char *name, xmlNsStack &nss)
{
    if (isEmptyString(name))
        name = tree.queryName();
    if (*name==':') //invalid xml but maybe valid ptree?
        name++;
    StringAttr prefix;
    const char *colon = strchr(name, ':');
    if (colon)
    {
        prefix.set(name, colon-name);
        name = colon+1;
    }
    xmlNodePtr node = xmlNewChild(parent, nullptr, (const xmlChar *)name, (const xmlChar *)tree.queryProp(nullptr));
    if (!node)
        return;
    xmlNsPtr ns = nullptr;
    if (tree.getAttributeCount())
    {
        Owned<IAttributeIterator> atts = tree.getAttributes();
        ForEach(*atts)
        {
            const char *attname = atts->queryName()+1;
            if (strncmp(attname, "xmlns", 5)==0)
            {
                attname+=5;
                if (*attname==':')
                    attname++;
             xmlNewNs(node, (const xmlChar *)atts->queryValue(), (const xmlChar *)attname);
            }
            else
                xmlNewProp(node, (const xmlChar *)attname, (const xmlChar *)atts->queryValue());
        }
    }
    if (prefix.isEmpty())
        node->ns = nullptr;
    else
    {
        xmlNsPtr ns = xmlSearchNs(node->doc, node, (const xmlChar *)prefix.str());
        if (!ns) //invalid but let's repair it
            ns = xmlNewNs(node, (const xmlChar *)"urn:hpcc:unknown", (const xmlChar *)prefix.str());
        node->ns = ns;
    }
    Owned<IPropertyTreeIterator> children = tree.getElements("*");
    ForEach(*children)
        addChildFromPtree(node, children->query(), nullptr, nss);
}

IPropertyTree *createPtreeFromXmlNode(xmlNodePtr node);

void copyXmlNode(IPropertyTree *tree, xmlNodePtr node)
{
    for(xmlAttrPtr att=node->properties;att!=nullptr; att=att->next)
        tree->setProp((const char *)att->name, (const char *)xmlGetProp(node, att->name));

    for (xmlNodePtr child=xmlFirstElementChild(node); child!=nullptr; child=xmlNextElementSibling(child))
        tree->addPropTree((const char *)child->name, createPtreeFromXmlNode(child));
}

IPropertyTree *createPtreeFromXmlNode(xmlNodePtr node)
{
    if (!node)
        return nullptr;
    Owned<IPropertyTree> tree = createPTree((const char *)node->name);
    copyXmlNode(tree, node);
    return tree.getClear();
}
class CEsdlScriptContext : public CInterfaceOf<IEsdlScriptContext>
{
private:
    void *espCtx = nullptr;
    xmlDocPtr doc = nullptr;
    xmlNodePtr root = nullptr;
    xmlXPathContextPtr xpathCtx = nullptr;

public:
    CEsdlScriptContext(void *ctx) : espCtx(ctx)
    {
        doc =   xmlParseDoc((const xmlChar *) "<esdl_script_context/>");
        xpathCtx = xmlXPathNewContext(doc);
        if(xpathCtx == nullptr)
            throw MakeStringException(-1, "CEsdlScriptContext: Unable to create new xPath context");

        root = xmlDocGetRootElement(doc);
        xpathCtx->node = root;
    }

private:
    ~CEsdlScriptContext()
    {
        xmlXPathFreeContext(xpathCtx);
        xmlFreeDoc(doc);
    }
    virtual void *queryEspContext() override
    {
        return espCtx;
    }
    xmlNodePtr getSectionNode(const char *name, const char *xpath="*[1]")
    {
        StringBuffer fullXpath(name);
        if (!isEmptyString(xpath))
            fullXpath.append('/').append(xpath);
        xmlNodePtr sect = nullptr;
        xmlXPathObjectPtr eval = xmlXPathEval((const xmlChar *) fullXpath.str(), xpathCtx);
        if (eval && XPATH_NODESET == eval->type && eval->nodesetval && eval->nodesetval->nodeNr && eval->nodesetval->nodeTab!=nullptr)
            sect = eval->nodesetval->nodeTab[0];
        xmlXPathFreeObject(eval);
        return sect;
    }
    void addXpathCtxConfigInputs(IXpathContext *tgtXpathCtx)
    {
        xmlXPathObjectPtr eval = xmlXPathEval((const xmlChar *) "config/*/Transform/Param", xpathCtx);
        if (!eval)
            return;
        if (XPATH_NODESET==eval->type && eval->nodesetval!=nullptr && eval->nodesetval->nodeNr!=0 && eval->nodesetval->nodeTab!=nullptr)
        {
            for (int i=0; i<eval->nodesetval->nodeNr; i++)
            {
                xmlNodePtr node = eval->nodesetval->nodeTab[i];
                if (node==nullptr)
                  continue;
                const char *name = (const char *)xmlGetProp(node,(const xmlChar *) "name");
                if (xmlHasProp(node,(const xmlChar *) "select"))
                  tgtXpathCtx->addInputXpath(name, (const char *) xmlGetProp(node,(const xmlChar *) "select"));
                else
                  tgtXpathCtx->addInputValue(name, (const char *) xmlGetProp(node,(const xmlChar *) "value"));
            }
        }
        xmlXPathFreeObject(eval);
    }
    const char *getProp(const char *xpath, StringBuffer &s) const override
    {
        xmlXPathObjectPtr eval = xmlXPathEval((const xmlChar *) xpath, xpathCtx);
        if (eval)
        {
            xmlChar *v = xmlXPathCastToString(eval);
            xmlXPathFreeObject(eval);
            if (v)
            {
                s.append((const char *)v);
                xmlFree(v);
                return s;
            }
        }
        return nullptr;
    }
    __int64 getPropInt64(const char *xpath, __int64 dft=0) const override
    {
        xmlXPathObjectPtr eval = xmlXPathEval((const xmlChar *) xpath, xpathCtx);
        if (eval)
        {
            xmlChar *val = xmlXPathCastToString(eval);
            xmlXPathFreeObject(eval);
            if (val && *val)
            {
                __int64 ret = _atoi64((const char *)val);
                xmlFree(val);
                return ret;
            }
        }
        return dft;
    }
    bool getPropBool(const char *xpath, bool dft=false) const override
    {
        xmlXPathObjectPtr eval = xmlXPathEval((const xmlChar *) xpath, xpathCtx);
        if (eval)
        {
            xmlChar *val = xmlXPathCastToString(eval);
            xmlXPathFreeObject(eval);
            if (val && *val)
            {
                bool ret = strToBool((const char *)val);
                xmlFree(val);
                return ret;
            }
        }
        return dft;
    }

    xmlNodePtr getSection(const char *name)
    {
        return getSectionNode(name, nullptr);
    }
    xmlNodePtr ensureSection(const char *name)
    {
        xmlNodePtr sect = getSection(name);
        if (sect)
            return sect;
        return xmlNewChild(root, nullptr, (const xmlChar *) name, nullptr);
    }
    void removeSection(const char *name)
    {
        xmlXPathObjectPtr eval = xmlXPathEval((const xmlChar *) name, xpathCtx);
        if (!eval || XPATH_NODESET != eval->type || xmlXPathNodeSetIsEmpty(eval->nodesetval))
            return;
        xmlNodeSetPtr ns = eval->nodesetval;
        int count = xmlXPathNodeSetGetLength(ns);
        for(int i=0; i<count; i++)
        {
            xmlNodePtr node = xmlXPathNodeSetItem(ns, i);
            if (node)
            {
                xmlUnlinkNode(node);
                xmlFreeNode(node);
            }
        }
    }
    xmlNodePtr replaceSection(const char *name)
    {
        removeSection(name);
        return xmlNewChild(root, nullptr, (const xmlChar *) name, nullptr);
    }
    virtual void setContent(const char *section, const char *xml) override
    {
        xmlNodePtr sect = replaceSection(section);
        if (xml==nullptr) //means delete content
            return;

        xmlParserCtxtPtr parserCtx = xmlCreateDocParserCtxt((const unsigned char *)xml);
        if (!parserCtx)
            throw MakeStringException(-1, "CEsdlScriptContext:setXmlSection: Unable to init parse of %s XML content", section);
        parserCtx->myDoc = doc;
        parserCtx->node = sect;
        xmlParseDocument(parserCtx);
        if (!parserCtx->wellFormed)
        {
           xmlFreeDoc(parserCtx->myDoc);
           parserCtx->myDoc = nullptr;
           xmlFreeParserCtxt(parserCtx);
           throw MakeStringException(-1, "CEsdlScriptContext:setXmlSection: Unable to parse %s XML content", section);
        }
        xmlFreeParserCtxt(parserCtx);
    }
    virtual void setContent(const char *section, IPropertyTree *tree) override
    {
        xmlNodePtr sect = replaceSection(section);
        if (tree==nullptr) //means delete content
            return;
        xmlNsStack nss;
        addChildFromPtree(sect, *tree, tree->queryName(), nss);
    }

    virtual void setAttribute(const char *section, const char *name, const char *value) override
    {
        xmlNodePtr sect = ensureSection(section);
        xmlNewProp(sect, (const xmlChar *)name, (const xmlChar *)value);
    }

    virtual const char *queryAttribute(const char *section, const char *name) override
    {
        xmlNodePtr sect = getSection(section);
        if (!sect)
            return nullptr;
        return (const char *) xmlGetProp(sect, (const xmlChar *)name);
    }

    virtual void toXML(StringBuffer &xml, const char *section, bool includeParentNode=false) override
    {
        xmlNodePtr sect = root;
        if (!isEmptyString(section))
        {
            sect = getSectionNode(section, includeParentNode ? nullptr : "*[1]");
            if (!sect)
                throw MakeStringException(-1, "CEsdlScriptContext:toXML: section not found %s", section);
        }

        xmlOutputBufferPtr xmlOut = xmlAllocOutputBuffer(nullptr);
        xmlNodeDumpOutput(xmlOut, sect->doc, sect, 0, 1, nullptr);
        xmlOutputBufferFlush(xmlOut);
        xmlBufPtr buf = (xmlOut->conv != nullptr) ? xmlOut->conv : xmlOut->buffer;
        if (xmlBufUse(buf))
            xml.append(xmlBufUse(buf), (const char *)xmlBufContent(buf));
        xmlOutputBufferClose(xmlOut);

    }
    virtual void toXML(StringBuffer &xml) override
    {
        toXML(xml, nullptr, true);
    }
    IXpathContext* createXpathContext(const char *section, bool strictParameterDeclaration) override
    {
        xmlNodePtr sect = getSectionNode(section);
        if (!sect)
            throw MakeStringException(-1, "CEsdlScriptContext:createXpathContext: section not found %s", section);
        CLibXpathContext *xpathContext = new CLibXpathContext(doc, sect, strictParameterDeclaration);
        xpathContext->addVariable("method", queryAttribute("esdl", "method"));
        xpathContext->addVariable("service", queryAttribute("esdl", "service"));
        xpathContext->addVariable("request", queryAttribute("esdl", "request"));

        //external parameters need <es:param> statements to make them accessible (in strict mode)
        addXpathCtxConfigInputs(xpathContext);
        if (!strictParameterDeclaration)
            xpathContext->declareRemainingInputs();

        return xpathContext;
    }
};

IEsdlScriptContext *createEsdlScriptContext(void * espCtx)
{
    return new CEsdlScriptContext(espCtx);
}

extern IXpathContext* getXpathContext(const char * xmldoc, bool strictParameterDeclaration)
{
    return new CLibXpathContext(xmldoc, strictParameterDeclaration);
}
