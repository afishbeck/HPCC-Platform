# HPCC Systems Certificates using JetStack cert-manager

This example demonstrates HPCC TLS configuration using Jetstack cert-manager.

## Jetstack cert-manager support:

The following wil use cert-manager to automatically provision and manage TLS certificates for the
HPCC.

The following steps can be used to set up cert-manager in a kubernetes cluster.

--------------------------------------------------------------------------------------------------------

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
helm install cert-manager jetstack/cert-manager --version v1.1.0
```

## Run from HPCC-Platform/helm directory

For now this example will assume you are in the helm directory of the HPCC-Systems source.

## Create a root certificate for our internal cluster certificate authority

This example uses OpenSSL to generate the root certificate for our internal cluster certificate authority.

We can create a root certificate and private key for our internal cluster certificate authority with
a single openssl call. This call uses the openssl config file found in the examples directory (ca-req.cfg).


```bash
openssl req -x509 -newkey rsa:2048 -nodes -keyout ca.key -sha256 -days 1825 -out ca.crt -config examples/certmanager/ca-req.cfg
```

For additonal information on the openssl command being used checkout this link:
https://www.openssl.org/docs/man1.0.2/man1/openssl-req.html

For a general overview check out this link:
https://www.golinuxcloud.com/create-certificate-authority-root-ca-linux


## Create a Kubernetes TLS secret from the generated root certificate and privatekey

The root certificate needs to be added as a kubernetes secret in order to be accessible to cert-manager.
The secret name matches the default name used in the internal issuer configuration in values.yaml.

```bash
kubectl create secret tls hpcc-internal-issuer-key-pair --cert=ca.crt --key=ca.key
```

## Installing the HPCC with certificate generation enabled

Install the HPCC helm chart with the "--set certificates.enabled" option set to true.

```bash
helm install myhpcc hpcc/ --set global.image.version=latest --set certificates.enabled=true
```

Check and see if the cerficate issuers have been successfully created.

```bash
kubectl get issuers -o wide
```

You should see something like this:

```bash
NAME                   READY   STATUS                AGE
hpcc-external-issuer   True                          3m57s
hpcc-internal-issuer   True    Signing CA verified   3m57s
```

Use kubectl to check the status of the deployed pods.  Wait until all pods are running before continuing.

```bash
kubectl get pods
```

The default external issuer uses self signed certificates. This makes it very easy to set up but browsers
will not recognize the certificates as trustworthy and the browser will warn users that the connection
is not safe.