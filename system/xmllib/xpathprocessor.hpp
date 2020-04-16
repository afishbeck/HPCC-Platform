 /*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2012 HPCC Systems®.

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

#ifndef XPATH_MANAGER_HPP_
#define XPATH_MANAGER_HPP_

#include "xmllib.hpp"
#include "jliball.hpp"

interface XMLLIB_API ICompiledXpath : public IInterface
{
public:

    virtual const char * getXpath() = 0;
};

interface XMLLIB_API IXpathContext : public IInterface
{
public:

    virtual bool addVariable(const char * name, const char * val) = 0;
    virtual bool addEvaluateCXVariable(const char * name, ICompiledXpath *xpath) = 0;
    virtual bool addEvaluateVariable(const char * name, const char * xpath) = 0;
    virtual bool addEvaluateCXParam(const char * name, ICompiledXpath *xpath) = 0;
    virtual bool addEvaluateParam(const char * name, const char * xpath) = 0;
    virtual const char * getVariable(const char * name, StringBuffer & variable) = 0;
    virtual bool evaluateAsBoolean(const char * xpath) = 0;
    virtual bool evaluateAsString(const char * xpath, StringBuffer & evaluated) = 0;
    virtual bool evaluateAsBoolean(ICompiledXpath * compiledXpath) = 0;
    virtual const char * evaluateAsString(ICompiledXpath * compiledXpath, StringBuffer & evaluated) = 0;
    virtual double evaluateAsNumber(ICompiledXpath * compiledXpath) = 0;
    virtual const char * getXpath() = 0;
    virtual bool setXmlDoc(const char * xmldoc) = 0;
    virtual void setUserData(void *) = 0;
    virtual void *getUserData() = 0;
    virtual void registerFunction(const char *xmlns, const char * name, void *f) = 0;
    virtual void registerNamespace(const char *prefix, const char *uri) = 0;
};

extern "C" XMLLIB_API ICompiledXpath* compileXpath(const char * xpath);
extern "C" XMLLIB_API IXpathContext*  getXpathContext(const char * xmldoc);

#endif /* XPATH_MANAGER_HPP_ */
