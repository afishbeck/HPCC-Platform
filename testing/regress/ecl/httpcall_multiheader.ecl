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

string TargetIP := '.' : stored('TargetIP');
string storedHeader := 'StoredHeaderDefault' : stored('StoredHeader');


httpEchoServiceResponseRecord :=
    RECORD
        string method{xpath('Method')};
        string path{xpath('UrlPath')};
        string parameters{xpath('UrlParameters')};
        set of string headers{xpath('Headers/Header')};
        string content{xpath('Content')};
    END;

string constHeader := 'constHeaderValue';
string hostURL := 'local:https://eclservices:8010/WsSmc/HttpEcho?name=doe,joe&number=1';

proxyResult := HTTPCALL(hostURL,'GET', 'text/xml', httpEchoServiceResponseRecord, xpath('Envelope/Body/HttpEchoResponse'), httpheader('literalHeader','literalValue'), httpheader('constHeader','constHeaderValue'), httpheader('storedHeader', storedHeader), httpheader('HPCC-Global-Id','9876543210'), httpheader('HPCC-Caller-Id','http222'));

output(proxyResult, named('proxyResult'));
