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

#include "espcontext.hpp"
#include "esdl_script.hpp"

//only support libxml2 for now

void addFeaturesToAccessMap(MapStringTo<SecAccessFlags> &accessmap, const char *s)
{
    StringArray entries;
    entries.appendList(s, ",");

    ForEachItemIn(i, entries)
    {
        StringArray pair;
        pair.appendList(entries.item(i), ":");
        if (pair.length()==0)
            continue;
        if (pair.length()==1)
            accessmap.setValue(pair.item(0), SecAccess_Read);
        else
        {
            SecAccessFlags required = getSecAccessFlagValue(pair.item(1));
            if (required >= SecAccess_None)
                accessmap.setValue(pair.item(0), required);
        }
    }
}

/**
 * validateFeaturesAccessFunction:
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 * Wraps IEspContext::validateFeaturesAccess()
 */
static void validateFeaturesAccessFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    if (!ctxt || !ctxt->context || !ctxt->context->userData)
    {
        xmlXPathSetError((ctxt), XPATH_INVALID_CTXT);
        return;
    }

    IEspContext *espContext = (IEspContext *)ctxt->context->userData;

    if (nargs != 1)
    {
        xmlXPathSetArityError(ctxt);
        return;
    }

    xmlChar *authstring = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt))
        return;

    MapStringTo<SecAccessFlags> accessmap;
    addFeaturesToAccessMap(accessmap, (const char *)authstring);

    bool ok = true;
    if (accessmap.ordinality()!=0)
        ok = espContext->validateFeaturesAccess(accessmap, false);

    if (authstring != nullptr)
        xmlFree(authstring);

    xmlXPathReturnBoolean(ctxt, ok ? 1 : 0);
}

/**
 * evaluateSecAccessFlagsFunction
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 */
static void secureAccessFlagsFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    if (!ctxt || !ctxt->context || !ctxt->context->userData)
    {
        xmlXPathSetError((ctxt), XPATH_INVALID_CTXT);
        return;
    }

    IEspContext *espContext = (IEspContext *)ctxt->context->userData;

    if (nargs == 0)
    {
        xmlXPathSetArityError(ctxt);
        return;
    }

    unsigned flags = 0;
    while(nargs--)
    {
        xmlChar *s = xmlXPathPopString(ctxt);
        if (xmlXPathCheckError(ctxt))
            return;
        if (s == nullptr)
        {
            xmlXPathSetArityError(ctxt);
            return;
        }
        SecAccessFlags f = getSecAccessFlagValue((const char *)s);
        xmlFree(s);
        if (f < SecAccess_None)
        {
            xmlXPathSetArityError(ctxt);
            return;
        }
        flags |= (unsigned)f;
    }

    xmlXPathReturnNumber(ctxt, flags);
}
/**
 * getFeatureSecAccessFlags
 * @ctxt:  an XPath parser context
 * @nargs:  the number of arguments
 *
 */
static void getFeatureSecAccessFlagsFunction (xmlXPathParserContextPtr ctxt, int nargs)
{
    if (!ctxt || !ctxt->context || !ctxt->context->userData)
    {
        xmlXPathSetError((ctxt), XPATH_INVALID_CTXT);
        return;
    }

    IEspContext *espContext = (IEspContext *)ctxt->context->userData;

    if (nargs != 1)
    {
        xmlXPathSetArityError(ctxt);
        return;
    }

    xmlChar *authstring = xmlXPathPopString(ctxt);
    if (xmlXPathCheckError(ctxt))
        return;
    if (authstring == nullptr)
    {
        xmlXPathSetArityError(ctxt);
        return;
    }

    SecAccessFlags access = SecAccess_None;
    espContext->authorizeFeature((const char *)authstring, access);
    xmlFree(authstring);

    xmlXPathReturnNumber(ctxt, access);
}

void registerEsdlXPathExtensions(IXpathContext *xpathContext, IEspContext *context)
{
    xpathContext->setUserData(context);
    xpathContext->registerNamespace("xsdl", "urn:hpcc:xsdl");
    xpathContext->registerFunction("urn:hpcc:xsdl", "validateFeaturesAccess", (void  *)validateFeaturesAccessFunction);
    xpathContext->registerFunction("urn:hpcc:xsdl", "secureAccessFlags", (void  *)secureAccessFlagsFunction);
    xpathContext->registerFunction("urn:hpcc:xsdl", "getFeatureSecAccessFlags", (void  *)getFeatureSecAccessFlagsFunction);
}
