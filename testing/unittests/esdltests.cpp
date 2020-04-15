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
   <xsdl:SetValue target="TransformValue"  value="'here'"/>
   <xsdl:choose>
      <xsdl:when test="not(esdl:validateFeaturesAccess('AllowSomething : Read, AllowAnother : Full'))">
         <xsdl:Fail code="401" message="concat('authorization failed for something or other (', $clientversion, ')')"/>
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
</xsdl:CustomRequestTransform>
)!!";

static const char *tgtcfg = "<method queryname='EchoPersonInfo'/>";

class ESDLTests : public CppUnit::TestFixture
{
    CPPUNIT_TEST_SUITE( ESDLTests );
        CPPUNIT_TEST(testEsdlTransformScripts);
    CPPUNIT_TEST_SUITE_END();

public:
    ESDLTests(){}

    void testEsdlTransformScripts()
    {
        try
        {
            Owned<IPropertyTree> cfg = createPTreeFromXMLString(tgtcfg);
            Owned<IPropertyTree> script = createPTreeFromXMLString(esdlScript);
            Owned<IEsdlCustomTransform> tf = createEsdlCustomTransform(*script);

            Owned<IEspContext> ctx = createEspContext(nullptr);//createHttpSecureContext(m_request.get()));

            StringBuffer request(soapRequest);
            tf->processTransform(ctx, cfg, "EsdlExample", "EchoPersonInfo", "EchoPersonInfoRequest", request, nullptr);
            fputs(request.str(), stdout);
        }
        catch (IWsException *E)
        {
            StringBuffer msg;
            fprintf(stdout, "Exception: %s", E->serialize(msg).str());
            E->Release();
        }
    }
};

CPPUNIT_TEST_SUITE_REGISTRATION( ESDLTests );
CPPUNIT_TEST_SUITE_NAMED_REGISTRATION( ESDLTests, "ESDL" );

#endif // _USE_CPPUNIT
