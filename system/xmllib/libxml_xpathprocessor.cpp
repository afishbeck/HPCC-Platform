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

#include "xpathprocessor.hpp"
#include "xmlerror.hpp"

#include <map>
#include <stack>
#include <memory>

class CLibCompiledXpath : public CInterface, public ICompiledXpath
{
private:
    xmlXPathCompExprPtr m_compiledXpathExpression = nullptr;
    StringBuffer m_xpath;
    ReadWriteLock m_rwlock;

public:
    IMPLEMENT_IINTERFACE;

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
};

static xmlXPathObjectPtr variableLookupFunc(void *data, const xmlChar *name, const xmlChar *ns_uri);

typedef std::map<std::string, xmlXPathObjectPtr> XPathObjectMap;
typedef std::pair<std::string, xmlXPathObjectPtr> XPathObjectPair;

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

class CLibXpathContext : public CInterface, public IXpathContext
{
private:
    xmlDocPtr m_xmlDoc = nullptr;
    xmlXPathContextPtr m_xpathContext = nullptr;
    StringBuffer m_xpath;
    ReadWriteLock m_rwlock;
    XPathScopeVector scopes;

public:
    IMPLEMENT_IINTERFACE;

    CLibXpathContext(const char * xmldoc) //not thread safe
    {
        setXmlDoc(xmldoc);
    }

    ~CLibXpathContext()
    {
        xmlXPathFreeContext(m_xpathContext);
        xmlFreeDoc(m_xmlDoc);
    }

    void beginScope(const char *name) override
    {
        scopes.emplace_back(new CLibXpathScope(name));
    }

    void endScope() override
    {
        scopes.pop_back();
    }

    static void tableScanCallback(void *payload, void *data, xmlChar *name)
    {
        DBGLOG("k/v == [%s,%s]\n", (char *) name, (char *) payload);
    }

    virtual void registerNamespace(const char *prefix, const char * uri) override
    {
        if (m_xpathContext)
            xmlXPathRegisterNs(m_xpathContext, (const xmlChar *) prefix, (const xmlChar *) uri);
    }

    virtual void registerFunction(const char *xmlns, const char * name, void *f) override
    {
        if (m_xpathContext)
            xmlXPathRegisterFuncNS(m_xpathContext, (const xmlChar *) name, (const xmlChar *) xmlns, (xmlXPathFunction) f);
    }

    virtual void setUserData(void *userdata) override
    {
        if (m_xpathContext)
            m_xpathContext->userData = userdata;
    }

    virtual void *getUserData() override
    {
        return (m_xpathContext) ? m_xpathContext->userData : nullptr;
    }

    virtual const char * getXpath() override
    {
        return m_xpath.str();
    }

    xmlXPathObjectPtr lookupVariable(const char *name, const char *ns_uri)
    {
        const char *fullname = name;
        StringBuffer s;
        if (!isEmptyString(ns_uri))
            fullname = s.append(ns_uri).append(':').append(name).str();

        xmlXPathObjectPtr obj = nullptr;
        for (XPathScopeVector::const_reverse_iterator it=scopes.crbegin(); !obj && it!=scopes.crend(); ++it)
            obj = xmlXPathObjectCopy(it->get()->getObject(fullname));

        //check root scope (library level) variables
        if (!obj)
            obj = xmlXPathObjectCopy((xmlXPathObjectPtr)xmlHashLookup2(m_xpathContext->varHash, (const xmlChar *)name, (const xmlChar *)ns_uri));
        return obj;
    }

    CLibXpathScope *queryScope(const char *scope)
    {
        auto count = scopes.size();
        if (!count)
            return nullptr;
        if (!*scope || streq(scope, "."))
            return scopes.back().get();
        if (streq(scope, ".."))
        {
            if (count<2)
                return nullptr;
            return scopes[count-2].get();
        }
        for (XPathScopeVector::reverse_iterator it=scopes.rbegin(); it!=scopes.rend(); ++it)
        {
            if (streq(it->get()->name, scope))
                return it->get();
        }
        return nullptr;
    }

    CLibXpathScope *queryExpectedScope(const char *scope, const char *expected)
    {
        CLibXpathScope *found = queryScope(scope);
        if (found)
        {
            if (!expected || streq(expected, found->name))
                return found;
        }
        return nullptr;
    }
    virtual bool scopeHasVariable(const char *scope, const char *name, const char *ns_uri, const char *expected) override
    {
        if (isEmptyString(name))
            return false;
        const char *fullname = name;
        StringBuffer s;
        if (!isEmptyString(ns_uri))
            fullname = s.append(ns_uri).append(':').append(name).str();
        if (!scope)
            return xmlHashLookup2(m_xpathContext->varHash, (const xmlChar *)name, (const xmlChar *)ns_uri)!=nullptr;
        CLibXpathScope *found = queryExpectedScope(scope, expected);
        if (!found)
            return false;
        return found->getObject(fullname)!=nullptr;
    }
    virtual bool addObjectVariable(const char * name, xmlXPathObjectPtr obj)
    {
        if (isEmptyString(name))
            return false;
        if (m_xpathContext)
        {
            if (!obj)
                throw MakeStringException(-1, "addObjectVariable %s error", name);
            if (!scopes.empty())
                return scopes.back()->setObject(name, obj);
            return xmlXPathRegisterVariable(m_xpathContext, (xmlChar *)name, obj) == 0;
        }
        return false;
    }

    virtual bool addVariable(const char * name,  const char * val) override
    {
        WriteLockBlock wblock(m_rwlock);
        if (!val)
            return false;
        return addObjectVariable(name, xmlXPathNewCString(val));
    }

    virtual bool addEvaluateVariable(const char * name, const char * xpath) override
    {
        if (isEmptyString(xpath))
            addVariable(name, "");
        if (m_xpathContext)
        {
            xmlXPathObjectPtr obj = evaluate(xpath);
            if (!obj)
                throw MakeStringException(-1, "addEvaluateVariable xpath error %s", xpath);
            return addObjectVariable(name, obj);
        }
        return false;
    }

    virtual bool addEvaluateCXVariable(const char * name, ICompiledXpath * compiled) override
    {
        if (!compiled)
            addVariable(name, "");
        if (m_xpathContext)
        {
            CLibCompiledXpath * clibCompiledXpath = static_cast<CLibCompiledXpath *>(compiled);
            xmlXPathObjectPtr obj = evaluate(clibCompiledXpath->getCompiledXPathExpression(), clibCompiledXpath->getXpath());
            if (!obj)
                throw MakeStringException(-1, "addEvaluateVariable xpath error %s", clibCompiledXpath->getXpath());
            return addObjectVariable(name, obj);
        }

        return false;
    }

    virtual const char * getVariable(const char * name, StringBuffer & variable) override
    {
        ReadLockBlock rblock(m_rwlock);
        if (m_xpathContext)
        {
            xmlXPathObjectPtr ptr = xmlXPathVariableLookupNS(m_xpathContext, (const xmlChar *)name, nullptr);
            if (!ptr)
                return nullptr;
            variable.append((const char *) ptr->stringval);
            xmlXPathFreeObject(ptr);
            return variable;
        }
        return nullptr;
    }

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

private:
    virtual bool setXmlDoc(const char * xmldoc) override
    {
        if (xmldoc && * xmldoc)
        {
            m_xmlDoc = xmlParseDoc((const unsigned char *)xmldoc);
            if (m_xmlDoc == nullptr)
            {
                ERRLOG("XpathProcessor:setxmldoc Error: Unable to parse XMLLib document");
                return false;
            }

            // Create xpath evaluation context
            m_xpathContext = xmlXPathNewContext(m_xmlDoc);
            if(m_xpathContext == nullptr)
            {
                ERRLOG("XpathProcessor:setxmldoc: Error: Unable to create new XMLLib XPath context");
                return false;
            }
            xmlXPathRegisterVariableLookup(m_xpathContext, variableLookupFunc, this);
            return true;
        }
        return false;
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

    virtual xmlXPathObjectPtr evaluate(xmlXPathCompExprPtr compiledXpath, const char* xpath)
    {
        xmlXPathObjectPtr evaluatedXpathObj = nullptr;
        if (compiledXpath)
        {
            ReadLockBlock rlock(m_rwlock);
            if ( m_xpathContext)
            {
                evaluatedXpathObj = xmlXPathCompiledEval(compiledXpath, m_xpathContext);
            }
            else
            {
                throw MakeStringException(XPATHERR_InvalidState,"XpathProcessor:evaluate: Error: Could not evaluate XPATH '%s'; ensure xmldoc has been set", xpath);
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

static xmlXPathObjectPtr variableLookupFunc(void *data, const xmlChar *name, const xmlChar *ns_uri)
{
    CLibXpathContext *ctxt = (CLibXpathContext *) data;
    if (!ctxt)
        return nullptr;
    return ctxt->lookupVariable((const char *)name, (const char *)ns_uri);
}

extern ICompiledXpath* compileXpath(const char * xpath)
{
    return new CLibCompiledXpath(xpath);
}

extern IXpathContext* getXpathContext(const char * xmldoc)
{
    return new CLibXpathContext(xmldoc);
}
