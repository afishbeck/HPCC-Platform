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

ESPenum AddressType : string
{
    HOME("Home"),
    WORK("Work"),
    HOTEL("Hotel")
};

ESPStruct NameInfo
{
    string First("Joe");
    string Last("Doe");
};

ESPStruct AddressInfo
{
    ESPenum AddressType type("Home");
    string Line1;
    string Line2;
    string City;
    string State;
    int Zip(33487);  
};

ESPrequest EchoPersonInfoRequest
{
     ESPstruct NameInfo Name;
     ESParray<ESPstruct AddressInfo> Addresses;
};

ESPresponse EchoPersonInfoResponse
{
     ESPstruct NameInfo Name;
     ESParray<ESPstruct AddressInfo> Addresses;
};

ESPservice [version("0.01")] EsdlExample
{
    ESPmethod EchoPersonInfo(EchoPersonInfoRequest, EchoPersonInfoResponse);
};

