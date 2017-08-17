/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2017 HPCC Systems®.

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

#OPTION('writeInlineContent', true);

L0SetRec := RECORD
   SET OF STRING shapes;
END;

ds := DATASET([{['<cst>Circle</cst>','<cst>Square</cst>','<cst>Triangle</cst>']}], L0SetRec);


output(ds, named('OrigInline'));
output(ds,,'REGRESS::TEMP::output_inline_set_xpath_orig.xml', overwrite, xml);

readWrittenOrig := dataset(DYNAMIC('REGRESS::TEMP::output_inline_set_xpath_orig.xml'), L0SetRec, xml('Dataset/Row'));
output(readWrittenOrig, named('readWrittenOrig'));

L1SetRec := RECORD
   SET OF STRING shapes {XPATH('Shapes/Shape/<>')};
END;

x := PROJECT(ds, L1SetRec);


output(x, named('NamedInline'));
output(x,,'REGRESS::TEMP::output_inline_set_xpath_named.xml', overwrite, xml);

L2SetRec := RECORD
   SET OF STRING shapes {XPATH('Shapes/Shape<>')};
END;

readWrittenNamed := dataset(DYNAMIC('REGRESS::TEMP::output_inline_set_xpath_named.xml'), L2SetRec, xml('Dataset/Row'));
output(readWrittenNamed, named('readWrittenNamed'));

y := PROJECT(x, L2SetRec);

output(y, named('UnnamedInline'));
output(y,,'REGRESS::TEMP::output_inline_set_xpath_unnamed.xml', overwrite, xml);

R2SetRec := RECORD
   SET OF STRING shapes {XPATH('Shapes/cst')};
END;

readWrittenUnnamed := dataset(DYNAMIC('REGRESS::TEMP::output_inline_set_xpath_unnamed.xml'), R2SetRec, xml('Dataset/Row'));
output(readWrittenUnnamed, named('readWrittenUnnamed'));


L3SetRec := RECORD
   SET OF STRING shapes {XPATH('Shapes/<>')};
END;

z := PROJECT(x, L3SetRec);

output(z, named('NonameInline'));
output(z,,'REGRESS::TEMP::output_inline_set_xpath_noname.xml', overwrite, xml);

readWrittenNoname := dataset(DYNAMIC('REGRESS::TEMP::output_inline_set_xpath_noname.xml'), R2SetRec, xml('Dataset/Row'));
output(readWrittenNoname, named('readWrittenNoname'));




