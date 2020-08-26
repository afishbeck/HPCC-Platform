ServiceOutRecord := RECORD
    string authenticated {XPATH('authenticated')};
END;

//call out using our http-connect-vaultsecret stored in our ecl vault

output(HTTPCALL('secret:vaultsecret','GET', 'application/json', ServiceOutRecord, XPATH('/'), LOG));

//call out using our http-connect-basicsecret.. ecl will look in kubernetes secrets first, then the ecl vault

output(HTTPCALL('secret:basicsecret','GET', 'application/json', ServiceOutRecord, XPATH('/'), LOG));


//call out using our http-connect-basicsecret.. but get it directly from the ecl vault

output(HTTPCALL('secret:myvault:basicsecret','GET', 'application/json', ServiceOutRecord, XPATH('/'), LOG));
