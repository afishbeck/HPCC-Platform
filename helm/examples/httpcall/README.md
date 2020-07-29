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

# Tell the vault command line application the server location (dev mode is http, default location is https)
export VAULT_ADDR=http://127.0.0.1:8200

## in a separate terminal window start vault port forwarding

kubectl port-forward vault-0 8200:8200

# Enabling kubernetes auth will allow k8s nodes to access the vault via their kubernetes.io access tokens.

vault auth enable kubernetes

## Configure vault kubernetes auth 

## Exec into the Vault pod:

kubectl exec -it vault-0 /bin/sh

# configure auth kubernetes
vault write auth/kubernetes/config \
   token_reviewer_jwt="$(cat /var/run/secrets/kubernetes.io/serviceaccount/token)" \
   kubernetes_host=https://${KUBERNETES_PORT_443_TCP_ADDR}:443 \
   kubernetes_ca_cert=@/var/run/secrets/kubernetes.io/serviceaccount/ca.crt

## exit from the vault-0 pod

# Setup hpcc-vault-access auth policy and role (using default service account)

vault policy write hpcc-kv-ro hpcc_vault_policies.hcl

vault write auth/kubernetes/role/hpcc-vault-access \
        bound_service_account_names=default \
        bound_service_account_namespaces=default \
        policies=hpcc-kv-ro \
        ttl=24h


# login to the vault command line using the vault token (development mode defaults to "root")

vault login
>Token (will be hidden): root

# create our example httpcall vault secret:

vault kv put secret/ecl/http-connect-basicsecret url=@url-basic username=@username password=@password
