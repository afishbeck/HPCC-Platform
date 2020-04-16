/*##############################################################################

    Copyright (C) 2020 HPCC Systems®.

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

#ifdef _USE_CPPUNIT
#include "unittests.hpp"
#include "espcontext.hpp"
#include "esdl_script.hpp"
#include "wsexcept.hpp"

#include <stdio.h>

// =============================================================== URI parser

static constexpr const char * soapRequest = R"!!(<?xml version="1.0" encoding="UTF-8"?>
<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
<soap:Body>
<extra>
<EchoPersonInfo>
  <_TransactionId>1736623372_3126765312_1333296170</_TransactionId>
  <Context>
   <Row>
    <Common>
     <TransactionId>1736623372_3126765312_1333296170</TransactionId>
    </Common>
   </Row>
  </Context>
  <EchoPersonInfoRequest>
   <Row>
    <Name>
     <First>Joe</First>
     <Last>Doe</Last>
    </Name>
    <Addresses>
     <Address>
      <type>Home</type>
      <Line1>101 Main street</Line1>
      <Line2>Apt 202</Line2>
      <City>Hometown</City>
      <State>HI</State>
      <Zip>96703</Zip>
     </Address>
    </Addresses>
   </Row>
  </EchoPersonInfoRequest>
</EchoPersonInfo>
</extra>
</soap:Body>
</soap:Envelope>
)!!";

static constexpr const char * esdlScript = R"!!(<xsdl:CustomRequestTransform target="soap:Body/extra/{$query}/{$request}">
   <xsdl:variable name="var1"  select="'script'"/>
   <xsdl:variable name="var2"  select="$var1"/>
   <xsdl:param name="param1" select="'script'"/>
   <xsdl:param name="param2" select="$param1"/>
   <xsdl:SetValue target="TestCase"  value="$testcase"/>
   <xsdl:SetValue target="Var2"  value="$var2"/>
   <xsdl:SetValue target="Param2"  value="$param2"/>
   <xsdl:choose>
      <xsdl:when test="not(xsdl:validateFeaturesAccess('AllowSomething : Read, AllowAnother : Full'))">
         <xsdl:Fail code="401" message="concat('authorization failed for something or other (', $clientversion, ')')"/>
      </xsdl:when>
      <xsdl:when test="not(xsdl:getFeatureSecAccessFlags('AllowSomething')=xsdl:secureAccessFlags('Full'))">
         <xsdl:Fail code="401" message="concat('auth flag check failed for something (', $clientversion, ')')"/>
      </xsdl:when>
      <xsdl:when test="('1'=$FailLevel1A)">
         <xsdl:Fail code="11" message="'FailLevel1A'"/>
      </xsdl:when>
      <xsdl:when test="('1'=$FailLevel1B)">
        <xsdl:if test="('1'=$FailLevel1B)">
           <xsdl:Fail code="12" message="'FailLevel1B'"/>
        </xsdl:if>
      </xsdl:when>
      <xsdl:when test="('1'=$AssertLevel1C)">
         <xsdl:Assert test="('0'=$AssertLevel1C)" code="13" message="'AssertLevel1C'"/>
      </xsdl:when>
      <xsdl:otherwise>
         <xsdl:SetValue target="InnerTestCase"  value="$testcase"/>
         <xsdl:choose>
            <xsdl:when test="('1'=$FailLevel2A)">
               <xsdl:Fail code="21" message="'FailLevel2A'"/>
            </xsdl:when>
            <xsdl:when test="('1'=$FailLevel2B)">
              <xsdl:if test="('1'=$FailLevel2B)">
                 <xsdl:Fail code="22" message="'FailLevel2B'"/>
              </xsdl:if>
            </xsdl:when>
            <xsdl:when test="('1'=$AssertLevel2C)">
               <xsdl:Assert test="('0'=$AssertLevel2C)" code="23" message="'AssertLevel2C'"/>
            </xsdl:when>
            <xsdl:otherwise>
               <xsdl:SetValue target="test"  value="'auth success'"/>
               <xsdl:SetValue target="Row/Name/Last" value="'XXX'"/>
               <xsdl:SetValue target="Row/Name/Last" value="'POE'"/>
               <xsdl:AppendValue target="Row/AppendTo"  value="'This'"/>
               <xsdl:AppendValue target="Row/AppendTo"  value="'One'"/>
               <xsdl:AppendValue target="Row/AppendTo"  value="'String'"/>
               <xsdl:AddValue target="Row/Name/Aliases/Alias"  value="'moe'"/>
               <xsdl:AddValue target="Row/Name/Aliases/Alias"  value="'poe'"/>
               <xsdl:AddValue target="Row/Name/Aliases/Alias"  value="'roe'"/>
            </xsdl:otherwise>
         </xsdl:choose>
      </xsdl:otherwise>
   </xsdl:choose>
</xsdl:CustomRequestTransform>
)!!";

static const char *target_config = "<method queryname='EchoPersonInfo'/>";

class ESDLTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( ESDLTests );
        CPPUNIT_TEST(testEsdlTransformScript);
        CPPUNIT_TEST(testEsdlTransformScriptVarParam);
        CPPUNIT_TEST(testEsdlTransformFailLevel1A);
        CPPUNIT_TEST(testEsdlTransformFailLevel1B);
        CPPUNIT_TEST(testEsdlTransformFailLevel1C);
        CPPUNIT_TEST(testEsdlTransformFailLevel2A);
        CPPUNIT_TEST(testEsdlTransformFailLevel2B);
        CPPUNIT_TEST(testEsdlTransformFailLevel2C);
    CPPUNIT_TEST_SUITE_END();

public:
    ESDLTests(){}

    void runTest(const char *config, const char *result, int code)
    {
        StringBuffer request(soapRequest);
        try
        {
            Owned<IPropertyTree> cfg = createPTreeFromXMLString(config);
            Owned<IPropertyTree> target = createPTreeFromXMLString(target_config);
            Owned<IPropertyTree> script = createPTreeFromXMLString(esdlScript);
            Owned<IEsdlCustomTransform> tf = createEsdlCustomTransform(*script);

            Owned<IEspContext> ctx = createEspContext(nullptr);//createHttpSecureContext(m_request.get()));
            tf->processTransform(ctx, target, "EsdlExample", "EchoPersonInfo", "EchoPersonInfoRequest", request, cfg);
            if (code)
                throw MakeStringException(99, "Test failed: expected an explicit exception %d", code);
            if (result && !streq(result, request.str()))
            {
                fputs(request.str(), stdout);
                throw MakeStringException(100, "Test failed");
            }
        }
        catch (IException *E)
        {
            StringBuffer m;
            if (code!=E->errorCode())
            {
                StringBuffer m;
                fprintf(stdout, "\nExpected %d Exception %d - %s\n", code, E->errorCode(), E->errorMessage(m).str());
                throw E;
            }
            E->Release();
        }
    }

    void testEsdlTransformScript()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='0'/>
    <Param name='AssertLevel1C' value='0'/>
    <Param name='FailLevel2A' value='0'/>
    <Param name='FailLevel2B' value='0'/>
    <Param name='AssertLevel2C' value='0'/>
  </Transform>
</config>)!!";

constexpr const char * result = R"!!(<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
 <soap:Body>
  <extra>
   <EchoPersonInfo>
    <Context>
     <Row>
      <Common>
       <TransactionId>1736623372_3126765312_1333296170</TransactionId>
      </Common>
     </Row>
    </Context>
    <_TransactionId>1736623372_3126765312_1333296170</_TransactionId>
    <EchoPersonInfoRequest>
     <TestCase>&apos;operations&apos;</TestCase>
     <InnerTestCase>&apos;operations&apos;</InnerTestCase>
     <Var2>script</Var2>
     <Row>
      <Addresses>
       <Address>
        <type>Home</type>
        <Line2>Apt 202</Line2>
        <Line1>101 Main street</Line1>
        <City>Hometown</City>
        <Zip>96703</Zip>
        <State>HI</State>
       </Address>
      </Addresses>
      <Name>
       <Last>POE</Last>
       <Aliases>
        <Alias>moe</Alias>
        <Alias>poe</Alias>
        <Alias>roe</Alias>
       </Aliases>
       <First>Joe</First>
      </Name>
      <AppendTo>ThisOneString</AppendTo>
     </Row>
     <Param2>script</Param2>
     <test>auth success</test>
    </EchoPersonInfoRequest>
   </EchoPersonInfo>
  </extra>
 </soap:Body>
</soap:Envelope>
)!!";

        runTest(config, result, 0);
    }

    void testEsdlTransformScriptVarParam()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='0'/>
    <Param name='AssertLevel1C' value='0'/>
    <Param name='FailLevel2A' value='0'/>
    <Param name='FailLevel2B' value='0'/>
    <Param name='AssertLevel2C' value='0'/>
    <Param name='param1' value='provided'/>
    <Param name='param2' select="concat('produced and ', $param1)"/>
    <Param name='var1' value='provided'/>
    <Param name='var2' select="concat('produced and ', $var1)"/>
  </Transform>
</config>)!!";

constexpr const char * result = R"!!(<soap:Envelope xmlns:soap="http://schemas.xmlsoap.org/soap/envelope/">
 <soap:Body>
  <extra>
   <EchoPersonInfo>
    <Context>
     <Row>
      <Common>
       <TransactionId>1736623372_3126765312_1333296170</TransactionId>
      </Common>
     </Row>
    </Context>
    <_TransactionId>1736623372_3126765312_1333296170</_TransactionId>
    <EchoPersonInfoRequest>
     <TestCase>&apos;operations&apos;</TestCase>
     <InnerTestCase>&apos;operations&apos;</InnerTestCase>
     <Var2>script</Var2>
     <Row>
      <Addresses>
       <Address>
        <type>Home</type>
        <Line2>Apt 202</Line2>
        <Line1>101 Main street</Line1>
        <City>Hometown</City>
        <Zip>96703</Zip>
        <State>HI</State>
       </Address>
      </Addresses>
      <Name>
       <Last>POE</Last>
       <Aliases>
        <Alias>moe</Alias>
        <Alias>poe</Alias>
        <Alias>roe</Alias>
       </Aliases>
       <First>Joe</First>
      </Name>
      <AppendTo>ThisOneString</AppendTo>
     </Row>
     <Param2>produced and provided</Param2>
     <test>auth success</test>
    </EchoPersonInfoRequest>
   </EchoPersonInfo>
  </extra>
 </soap:Body>
</soap:Envelope>
)!!";

        runTest(config, result, 0);
    }

    void testEsdlTransformFailLevel1A()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='1'/>
    <Param name='FailLevel1B' value='1'/>
    <Param name='AssertLevel1C' value='1'/>
    <Param name='FailLevel2A' value='1'/>
    <Param name='FailLevel2B' value='1'/>
    <Param name='AssertLevel2C' value='1'/>
  </Transform>
</config>)!!";

        runTest(config, nullptr, 11);
    }

    void testEsdlTransformFailLevel1B()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='1'/>
    <Param name='AssertLevel1C' value='1'/>
    <Param name='FailLevel2A' value='1'/>
    <Param name='FailLevel2B' value='1'/>
    <Param name='AssertLevel2C' value='1'/>
  </Transform>
</config>)!!";

        runTest(config, nullptr, 12);
    }

    void testEsdlTransformFailLevel1C()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='0'/>
    <Param name='AssertLevel1C' value='1'/>
    <Param name='FailLevel2A' value='1'/>
    <Param name='FailLevel2B' value='1'/>
    <Param name='AssertLevel2C' value='1'/>
  </Transform>
</config>)!!";

        runTest(config, nullptr, 13);
    }

    void testEsdlTransformFailLevel2A()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='0'/>
    <Param name='AssertLevel1C' value='0'/>
    <Param name='FailLevel2A' value='1'/>
    <Param name='FailLevel2B' value='1'/>
    <Param name='AssertLevel2C' value='1'/>
  </Transform>
</config>)!!";

        runTest(config, nullptr, 21);
    }

    void testEsdlTransformFailLevel2B()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='0'/>
    <Param name='AssertLevel1C' value='0'/>
    <Param name='FailLevel2A' value='0'/>
    <Param name='FailLevel2B' value='1'/>
    <Param name='AssertLevel2C' value='1'/>
  </Transform>
</config>)!!";

        runTest(config, nullptr, 22);
    }

    void testEsdlTransformFailLevel2C()
    {
        constexpr const char *config = R"!!(<config>
  <Transform>
    <Param name='testcase' value="'operations'"/>
    <Param name='FailLevel1A' value='0'/>
    <Param name='FailLevel1B' value='0'/>
    <Param name='AssertLevel1C' value='0'/>
    <Param name='FailLevel2A' value='0'/>
    <Param name='FailLevel2B' value='0'/>
    <Param name='AssertLevel2C' value='1'/>
  </Transform>
</config>)!!";

        runTest(config, nullptr, 23);
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( ESDLTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( ESDLTests, "ESDL" );

#endif // _USE_CPPUNIT
