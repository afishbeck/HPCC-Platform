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

//#OPTION('writeInlineContent', true);  explicitly ignoring writing inline xpaths ('<>')

inline0Rec := RECORD
   string color;
   string shape {xpath('Shape')};
   string texture;
END;

L0Rec := RECORD
   DATASET(inline0Rec) peeps;
END;

ds := DATASET([{[
        {'<rgb>Red</rgb>','<cst>Circle</cst>','<sss>Sandy</sss>'},
        {'<rgb>Green</rgb>','<cst>Square</cst>','<sss>Smooth</sss>'},
        {'<rgb>Blue</rgb>','<cst>Triangle</cst>','<sss>Spikey</sss>'}
   ]}], L0Rec);


output(ds, named('OrigInline'));
output(ds,,'REGRESS::TEMP::output_ignore_inline_xpath_orig.xml', overwrite, xml);

readWrittenOrig := dataset(DYNAMIC('REGRESS::TEMP::output_ignore_inline_xpath_orig.xml'), L0Rec, xml('Dataset/Row'));
output(readWrittenOrig, named('readWrittenOrig'));

inline1Rec := RECORD
   string color;
   string shape {xpath('Shape/<>')};
   string texture;
END;

L1Rec := RECORD
   DATASET(inline1Rec) peeps{xpath('L1/L2/<>')};
END;

x := PROJECT(ds, L1Rec);


output(x, named('NamedInline'));
output(x,,'REGRESS::TEMP::output_ignore_inline_xpath_named.xml', overwrite, xml);

readWrittenNamed := dataset(DYNAMIC('REGRESS::TEMP::output_ignore_inline_xpath_named.xml'), L1Rec, xml('Dataset/Row'));
output(readWrittenNamed, named('readWrittenNamed'));

inline2Rec := RECORD
   string color;
   string shape {xpath('Shape<>')};
   string texture;
END;

L2Rec := RECORD
   DATASET(inline2Rec) peeps{xpath('L1/L2<>')}; //should be ignored on output
END;

y := PROJECT(x, L2Rec);

output(y, named('UnnamedInline'));
output(y,,'REGRESS::TEMP::output_ignore_inline_xpath_unnamed.xml', overwrite, xml);

read2Rec := RECORD
   string color;
   string shape {xpath('Shape')};
   string texture;
END;

R2Rec := RECORD
   DATASET(read2Rec) peeps{xpath('L1/L2')};
END;

readWrittenUnnamed := dataset(DYNAMIC('REGRESS::TEMP::output_ignore_inline_xpath_unnamed.xml'), R2Rec, xml('Dataset/Row'));
output(readWrittenUnnamed, named('readWrittenUnnamed'));


inline3Rec := RECORD
   string color;
   string shape {xpath('<>')};
   string texture;
END;

L3Rec := RECORD
   DATASET(inline3Rec) peeps{xpath('L1/<>')}; //for dataset inline part should be ignored
END;

z := PROJECT(x, L3Rec);

output(z, named('NonameInline'));
output(z,,'REGRESS::TEMP::output_ignore_inline_xpath_noname.xml', overwrite, xml);

read3Rec := RECORD
   string color;
   string shape; //was written unamed unencoded
   string texture;
END;

R3Rec := RECORD
   DATASET(read3Rec) peeps{xpath('L1')};
END;


readWrittenNoname := dataset(DYNAMIC('REGRESS::TEMP::output_ignore_inline_xpath_noname.xml'), R3Rec, xml('Dataset/Row'));
output(readWrittenNoname, named('readWrittenNoname'));

