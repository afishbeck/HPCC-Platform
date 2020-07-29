ServiceOutRecord := RECORD
    string authenticated {XPATH('authenticated')};
END;

output(HTTPCALL('secret:vaultsecret','GET', 'application/json', ServiceOutRecord, XPATH('/'), LOG));
