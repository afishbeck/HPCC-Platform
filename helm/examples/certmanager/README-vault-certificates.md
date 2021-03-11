# Install Hashicorp Vault

See also https://learn.hashicorp.com/tutorials/vault/kubernetes-cert-manager for a more Vault centric tutorial on setting up cert-manager with vault.

## Add hashicorp to you helm repo
```bash
helm repo add hashicorp https://helm.releases.hashicorp.com
"hashicorp" has been added to your repositories
```

## Helm install hashicorp vault

Disable the vault sidecar injector by setting "injector.enabled=false".

```bash
helm install vault hashicorp/vault --set "injector.enabled=false"
NAME: vault
## ...
```

Check the pods:
```bash
kubectl get pods
```

Vault pods should be running, but not ready

## Initialize and unseal the vault

Initialize Vault with one key share and one key threshold.  Saving off the output in json format so
we can utilize the unseal key and root token later.

```bash
kubectl exec vault-0 -- vault operator init -key-shares=1 -key-threshold=1 -format=json > init-keys.json
```

View the unseal key found in init-keys.json.

```bash
$ cat init-keys.json | jq -r ".unseal_keys_b64[]"
```

Create an environment variable holding the unseal key:

```bash
VAULT_UNSEAL_KEY=$(cat init-keys.json | jq -r ".unseal_keys_b64[]")
```

Unseal Vault running on the vault-0 pod with the $VAULT_UNSEAL_KEY.

```bash
kubectl exec vault-0 -- vault operator unseal $VAULT_UNSEAL_KEY
```

Check the pods:
```bash
kubectl get pods
```

Vault pods should now be running and ready.

## Configure the Vault PKI secrets engine (certificate authority)

View the vault root token:
```bash
cat init-keys.json | jq -r ".root_token"
```

Create a variable named VAULT_ROOT_TOKEN to capture the root token.
```bash
$ VAULT_ROOT_TOKEN=$(cat init-keys.json | jq -r ".root_token")
```

Login to Vault running on the vault-0 pod with the $VAULT_ROOT_TOKEN.
```bash
$ kubectl exec vault-0 -- vault login $VAULT_ROOT_TOKEN
```

Start an interactive shell session on the vault-0 pod.
```bash
$ kubectl exec --stdin=true --tty=true vault-0 -- /bin/sh
```
We are now working from the vault-0 pod.

Enable the PKI secrets engine at its default path.
```bash
/$ vault secrets enable pki
```

Configure the max lease time-to-live (TTL) to 8760h.
```bash
$ vault secrets tune -max-lease-ttl=8760h pki
```

# Vault CA key pair

Vault can accept an existing key pair, or it can generate its own self-signed root. In general, they recommend maintaining your root CA outside of Vault and providing Vault a signed intermediate CA, but for this demo we will keep it simple and generate a self signed root certificate.

Generate a self-signed certificate valid for 8760h.
```bash
$ vault write pki/root/generate/internal common_name=example.com ttl=8760h
```

Configure the PKI secrets engine certificate issuing and certificate revocation list (CRL) endpoints to use the Vault service in the default namespace.
```bash
$ vault write pki/config/urls issuing_certificates="http://vault.default:8200/v1/pki/ca" crl_distribution_points="http://vault.default:8200/v1/pki/crl" 
```

For our internal MTLS certificates we will use our kubernetes namespace as our domain name. This will allow us to recongize where these components reside.
For our external TLS certificates for this demo we will use myhpcc.com as our domain.

Configure a role named hpccnamespace that enables the creation of certificates hpccnamespace domain with any subdomains.

$ vault write pki/roles/hpccnamespace \
    allowed_domains=hpccnamespace \
    allow_subdomains=true \
    max_ttl=72h

Configure a role named myhpcc-dot-com that enables the creation of certificates myhpcc.com domain with any subdomains.

$ vault write pki/roles/myhpcc-dot-com \
    allowed_domains=myhpcc.com \
    allow_subdomains=true \
    max_ttl=72h

Create a policy named pki that enables read access to the PKI secrets engine paths.

```bash
$ vault policy write pki - <<EOF
path "pki*"                        { capabilities = ["read", "list"] }
path "pki/roles/myhpcc-dot-com"   { capabilities = ["create", "update"] }
path "pki/sign/myhpcc-dot-com"    { capabilities = ["create", "update"] }
path "pki/issue/myhpcc-dot-com"   { capabilities = ["create"] }
path "pki/roles/hpccnamespace"   { capabilities = ["create", "update"] }
path "pki/sign/hpccnamespace"    { capabilities = ["create", "update"] }
path "pki/issue/hpccnamespace"   { capabilities = ["create"] }
EOF
```

Configure Kubernetes authentication
Vault provides a Kubernetes authentication method that enables clients to authenticate with a Kubernetes Service Account Token.

Enable the Kubernetes authentication method.

```bash
$ vault auth enable kubernetes
```

Configure the Kubernetes authentication method to use the service account token, the location of the Kubernetes host, and its certificate.

```bash
$ vault write auth/kubernetes/config \
    token_reviewer_jwt="$(cat /var/run/secrets/kubernetes.io/serviceaccount/token)" \
    kubernetes_host="https://$KUBERNETES_PORT_443_TCP_ADDR:443" \
    kubernetes_ca_cert=@/var/run/secrets/kubernetes.io/serviceaccount/ca.crt
```

Finally, create a Kubernetes authentication role named issuer that binds the pki policy with a Kubernetes service account named issuer.

```bash
$ vault write auth/kubernetes/role/issuer \
    bound_service_account_names=issuer \
    bound_service_account_namespaces=cert-manager,hpccnamespace \
    policies=pki \
    ttl=20m
```
Exit from the vault pod:

```bash
exit
```

Deploy Cert Manager

---From CA example


Configure an issuer and generate a certificate
The cert-manager enables you to define Issuers that interface with the Vault certificate generating endpoints. These Issuers are invoked when a Certificate is created.

Create a namespace named cert-manager to host the cert-manager.

$ kubectl create namespace cert-manager
## Install cert-manager custom resource defintions:

This adds new custom resource types to kubernetes for certificate issuers and certificates.

```
kubectl apply -f https://github.com/jetstack/cert-manager/releases/download/v1.1.0/cert-manager.crds.yaml
```

## Install cert-manager helm chart:

Add Jetstack helm repo:

```bash
helm repo add jetstack https://charts.jetstack.io
```

Install vault server.

```bash
helm install cert-manager jetstack/cert-manager --namespace cert-manager --version v1.1.0
```

Create a service account named issuer within the default namespace.

```bash
$ kubectl create serviceaccount issuer
```

The service account generated a secret that is required by the Issuer.

Get all the secrets in the default namespace.

```bash
$ kubectl get secrets
```

Create a variable named ISSUER_SECRET_REF to capture the secret name.

```bash
$ ISSUER_SECRET_REF=$(kubectl get serviceaccount issuer -o json | jq -r ".secrets[].name")
```

## Installing the HPCC with certificate generation enabled

Install the HPCC helm chart with the "--set certificates.enabled" option set to true.

```bash
helm install myhpcc hpcc/ --set global.image.version=latest --set certificates.enabled=true --set certificates.issuers.local.spec.vault.auth.kubernetes.secretRef.name=$ISSUER_SECRET_REF  --set certificates.issuers.external.spec.vault.auth.kubernetes.secretRef.name=$ISSUER_SECRET_REF
```

