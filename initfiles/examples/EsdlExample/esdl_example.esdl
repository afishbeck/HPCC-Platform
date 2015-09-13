/*##############################################################################

    HPCC SYSTEMS software Copyright (C) 2015 HPCC Systems.

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

////////////////////////////////////////////////////////////

ESPStruct NameInfo
{
    string First("Joe");
    string Last("Doe");
};

ESPStruct AddressInfo
{
    string Line1;
    string Line2;
    string City;
    string State;
    int Zip(33487);  
};

ESPrequest EchoPersonInfoRequest
{
     ESPstruct NameInfo Name;
     ESPstruct AddressInfo Address;
};

ESPresponse EchoPersonInfoResponse
{
     ESPstruct NameInfo Name;
     ESPstruct AddressInfo Address;
};

ESPservice [version("0.01")] EsdlExample
{
    ESPmethod EchoPersonInfo(EchoPersonInfoRequest, EchoPersonInfoResponse);
};

