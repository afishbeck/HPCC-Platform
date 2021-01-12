responseRecord :=
    RECORD
        string method{xpath('Method')};
        string path{xpath('UrlPath')};
        string parameters{xpath('UrlParameters')};
        set of string headers{xpath('Headers/Header')};
        string content{xpath('Content')};
    END;

//local: tells HTTPCALL/SOAPCALL to use our local client certificate
//
string hostURL := 'local:https://eclservices:8010/WsSmc/HttpEcho?name=doe,joe&number=1';

echoResult := HTTPCALL(hostURL,'GET', 'text/xml', responseRecord, xpath('Envelope/Body/HttpEchoResponse'));
output(echoResult, named('localHttpEchoResult'));
