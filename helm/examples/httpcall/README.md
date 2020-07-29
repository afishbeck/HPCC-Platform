# Containerized HPCC Systems HTTPCALL and SOAPCALL Secrets

# HPCC Systems HTTPCALL Secrets

# About HTTP-CONNECT Secrets:

HTTP-CONNECT secrets consist of a url string and optional additional secrets associated with that URL.  If we supported providing credentials
without an associated url then those credentials could be sent anywhere and wouldn't be secret very long.

Besides the URL values can currently be set for proxy (tusted for keeping these secrets), username, and password.

# Creating the HTTP-CONNECT Secrets

Example basic auth secret:

kubectl create secret generic http-connect-basicsecret --from-file=url=url-basic --from-file=username --from-file=password

Example client certificate secret:

kubectl create secret generic http-connect-certsecret --from-file=url=url-cert  --from-file=tls.key --from-file=tls.crt


# HELM Installing the HPCC with the HTTP-CONNECT Secrets added to ECL components

Install the HPCC helm chart with the secrets just defined added to all components that run ECL.

helm install httpconnect hpcc/ --set global.image.version=latest -f examples/httpcall/values-http-connect.yaml


# ECL test

ecl run hthor httpcall.ecl



# Example Hashicorp Vault support:


# Install hashicorp vault in dev mode.  This is for development only, never deploy this way in production
# Deploying in dev mode sets up an in memory kv store that won't persist secret values across restart,
# and the vault will automatically be unsealed.  In my tests the default root token was "root".

helm install vault hashicorp/vault --set "server.dev.enabled=true"

# Enabling kubernetes auth will allow k8s nodes to access the vault via their kubernetes.io access tokens.

vault auth -address=http://localhost:8200 enable kubernetes

# login to the vault command line using the vault token (development mode defaults to "root")

vault login -address=http://localhost:8200
vault login -address=http://localhost:8200
>Token (will be hidden): root

# create our example httpcall vault secret:

vault kv put -address=http://localhost:8200 secret/esp/http-connect-vaultbasicsecret url=@url-basic username=@username password=@password
