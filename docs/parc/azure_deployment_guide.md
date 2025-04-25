# Overview

-   Purpose: This document outlines the high-level steps for deploying a Parc Azure server locally
    and in a Azure kubernetes cluster
    -   Note: This guide outlines one method for deploying the Parc server on Azure, but alternative
        approaches exist, such as using the Azure portal instead of the Azure CLI. and in Azure.
-   Server Overview: [Parc Azure Reference Server Overview](azure_reference_server_overview.md)

# Prerequisites

Before you begin, ensure you have to following:

-   **Azure Account**
    -   An active Azure subscription with permissions to create and manage resources.
-   **Tools**
    -   [**Docker**](https://docs.docker.com/engine/install/)**:** Installed and running for
        building/managing container images.
    -   [**Kubectl**](https://kubernetes.io/docs/tasks/tools/install-kubectl-linux/#install-kubectl-binary-with-curl-on-linux)**:**
        Installed for interacting with the kubernetes cluster.
    -   [**Azure CLI**](https://learn.microsoft.com/en-us/cli/azure/install-azure-cli)**:**
        Installed for running specific commands to create/manage Azure resources.

# Build and Run Parc Azure Server

## Pre-Server Deployment

```shell
git clone https://github.com/privacysandbox/data-plane-shared-libraries.git
```

## Build Parc Azure Server

Change to the `parc/azure/` directory, build the Parc Azure Server image:

```shell
docker buildx build \
  --tag=privacysandbox/parc/parc-azure-server:latest \
  --build-context repo-root=../.. \
  .
```

### Required Server Flags

The server has a couple required flags:

-   `parameter_file_path`
-   `blob_http_endpoint`

See [server flags](#server-flags) for detail and description.

### Key Flags

The Parc Azure server reads data from two sources:

#### `parameter_file_path`

A local jsonl file containing parameters in the form of key-value pairs

```json
{ "key": "parc-functional-test-param1", "value": "test-value1" }
{ "key": "parc-functional-test-param2", "value": "test-value2" }
```

#### `blob_http_endpoint`

An
[Azure Storage Account](https://learn.microsoft.com/en-us/azure/storage/common/storage-account-overview)
for Blob data

Azure Azurite Blob Storage example for local deployment:

```shell
http://"${AZURITE_HOSTNAME}":"${AZURITE_PORT}"
```

Azure Blob Storage example for cloud deployment:

```shell
https://"${STORAGE_ACCOUNT_NAME}".blob.core.windows.net
```

## Run the Parc Server

### Run the Parc Azure Server locally

#### Configure Data Source for local deployment

When running the Parc server in a local deployment, you have the flexibility to configure its data
source. The server can be set up to request data either from a local Azurite test server, which
emulates Azure Storage services on your machine, or directly from a live Azure Blob Storage account
in the cloud. This choice directly affects the server's configuration; specifically, the URL
provided via the `blob_http_endpoint` flag must point to the appropriate endpoint. Either the local
address and port used by your Azurite instance or endpoint URL of your actual Azure Blob Storage
account.

#### Command:

```shell
docker run -it --rm \
    --entrypoint=/server/bin/parc-azure-cpp \
    --mount type=bind,source=parameters.jsonl,target=/etc/parameters.jsonl \
    privacysandbox/parc/parc-azure-server:latest \
    --address=0.0.0.0 \
    --port=51337 \
    --parameters_file_path=/etc/parameters.jsonl \
    --blob_http_endpoint=http://"${AZURITE_HOSTNAME}":"${AZURITE_PORT}" \
    --account_name="${AZURE_ACCT}" \
    --account_key="${AZURE_ACCT_KEY}" \
    --blob_chunk_size=8388608 \
    --verbose
```

### Run Parc Azure Server on Azure Kubernetes Service (AKS)

The Parc Azure server is intended to be run as part of a Kubernetes cluster deployment in AKS

#### Create a Resource Group

##### _Azure CLI_

1. Create resource group:

    ```shell
    az group create --name "${MY_RESOURCE_GROUP_NAME}"  --location "${MY_REGION}"
    ```

1. To verify run:

    ```shell
    az group show --name "${MY_RESOURCE_GROUP_NAME}"
    ```

    Your output should look similar to this:

    ```json
    {
        "id": "/subscriptions/<subscription_id>/resourceGroups/<resource-group-name>",
        "location": "<region>",
        "managedBy": null,
        "name": "<resource-group-name>",
        "properties": {
            "provisioningState": "Succeeded"
        },
        "tags": {},
        "type": "Microsoft.Resources/resourceGroups"
    }
    ```

#### Create Azure Container Registry

Note: The use of Azure Container Registry specifically is not a requirement. Any container image
registry will suffice.

##### _Azure CLI_

1. Create an Azure Container Registry:

    ```shell
    az acr create \
    --resource-group "${MY_RESOURCE_GROUP_NAME}" \
    --name "${MY_CONTAINER_REGISTRY_NAME}" \
    --sku Standard \
    --admin-enabled false
    ```

1. Create a
   [permissions token](https://learn.microsoft.com/en-us/azure/container-registry/container-registry-repository-scoped-permissions)
   (per the last option
   [here](https://learn.microsoft.com/en-us/azure/container-registry/container-registry-authentication?tabs=azure-cli#authentication-options)):

    Note: If you do have a permission token you can not write to the container registry

    ```shell
    az acr token create \
    --name "${MY_CR_TOKEN_NAME}" \
    --registry "${MY_CONTAINER_REGISTRY_NAME}" \
    --repository samples/hello-world content/write content/read \
    --output json
    ```

    Your output should look similar to this:

    ```json
    {
        "adminUserEnabled": false,
        "creationDate": "2019-01-08T22:32:13.175925+00:00",
        "id": "/subscriptions/<subscription_id>/resourceGroups/<resource-group-name>/providers/Microsoft.ContainerRegistry/registries/<container-registry-name>",
        "location": "<region>",
        "loginServer": "<container-registry-name>.azurecr.io",
        "name": "<container-registry-name>",
        "provisioningState": "Succeeded",
        "resourceGroup": "<resource-group-name>",
        "sku": {
            "name": "Basic",
            "tier": "Basic"
        },
        "status": null,
        "storageAccount": null,
        "tags": {},
        "type": "Microsoft.ContainerRegistry/registries"
    }
    ```

1. From the above command, note the login password. Run the following:

    ```shell
    docker login "${MY_CONTAINER_REGISTRY_NAME}".azurecr.io -u "${MY_CR_TOKEN_NAME}" -p "${PASSWORD}"
    ```

1. How to push and pull images from registry:

    ```shell
    "${MY_CONTAINER_REGISTRY_NAME}".azurecr.io/<docker-image:tag>

    docker push "${MY_CONTAINER_REGISTRY_NAME}".azurecr.io/<docker-image:tag>

    az acr manifest list-metadata "${MY_CONTAINER_REGISTRY_NAME}".azurecr.io/<docker-image:tag>

    docker run <container-registry-name>.azurecr.io/<docker-image:tag>
    ```

    To verify, find your Azure Container Registry in the Azure Portal and look under its
    **Repository** section; your pushed image should be listed there

#### Create a Kubernetes Cluster on AKS

##### _Azure CLI_

1. Create an AKS Cluster with OIDC Enabled:

    ```shell
    az aks create \
        --resource-group "${MY_RESOURCE_GROUP_NAME}" \
        --name "${MY_AKS_CLUSTER_NAME}" \
        --node-count 1 \
        --enable-oidc-issuer \
        --generate-ssh-keys
    ```

    Note: \--enable-oidc-issuer is optional can be enabled later

1. Configure kubectl to connect to your Kubernetes cluster using the az aks get-credentials command:

    ```shell
    az aks get-credentials \
    --resource-group "${MY_RESOURCE_GROUP_NAME}" \
    --name "${MY_AKS_CLUSTER_NAME}"
    ```

1. Verify the connection to your cluster:

    ```shell
    kubectl get nodes
    ```

    ```shell
    az aks list --resource-group "${MY_RESOURCE_GROUP_NAME}" -o table
    ```

    Your output should look similar to this:

    ```text

    Name              Location    ResourceGroup    KubernetesVersion    CurrentKubernetesVersion    ProvisioningState    Fqdn
    ----------------  ----------  ---------------  -------------------  --------------------------  -------------------  --------------------------------------------------
    aks-cluster-name  eastus      <resource-group-name>          1.30.7               1.30.7                      Succeeded            aks-cluster-name-dns-n82whh8g.hcp.eastus.azmk8s.io`

    ```

#### Create User-Assigned Managed Identity

##### _Azure CLI_

1. Create User-Assigned Managed Identity

    ```shell
    az identity create \
        -g "${MY_RESOURCE_GROUP_NAME}" \
        -n "${USER_ASSIGNED_IDENTITY_NAME}"
    ```

    To verify run:

    ```shell
    az identity list -g "${MY_RESOURCE_GROUP_NAME}"
    ```

#### Enable OpenID Connect (OIDC) for AKS Cluster

OIDC extends the OAuth 2.0 authorization protocol for use as another authentication protocol and is
necessary when creating a Federated Credential

If OIDC was not enabled during the creation of AKS Cluster run this command:

```shell
az aks update \
    --name "${MY_AKS_CLUSTER_NAME}" \
    --resource-group "${MY_RESOURCE_GROUP_NAME}" \
    --enable-oidc-issuer
```

#### Create Federated Credential for User-Assigned Managed Identity

This allows the Kubernetes Cluster to be modified which is essential for Parc Azure Server
communication within the Kubernetes Cluster

1. Retrieve OIDC Issuer ID URL

    ```shell
    az aks show \
        --name "${MY_AKS_CLUSTER_NAME}" \
        --resource-group "${MY_RESOURCE_GROUP_NAME}" \
        --query "oidcIssuerProfile.issuerUrl" -o tsv
    ```

2. Retrieve User Assigned Managed Identity ID and Principal

    Client ID:

    ```shell
    az identity show \
        --name "${USER_ASSIGNED_IDENTITY_NAME}" \
        --resource-group "${MY_RESOURCE_GROUP_NAME}" \
        --query 'clientId' -o tsv
    ```

    Principal ID:

    ```shell
    az identity show \
        --name "${USER_ASSIGNED_IDENTITY_NAME}" \
        --resource-group "${MY_RESOURCE_GROUP_NAME}" \
        --query 'principalId' -o tsv
    ```

3. Grant Managed Identity Permissions to an azure store account

    ```shell
    # Get Storage Account Resource ID
    export STORAGE_ACCOUNT_ID=$(az storage account show --name "${YOUR_STORAGE_ACCOUNT_NAME}" --resource-group <storagerg> --query id -o tsv)

    # Assign role, ensure principal ID is correct!
    az role assignment create --role "Storage Blob Data Owner" \ --assignee-object-id ${USER_ASSIGNED_PRINCIPAL_ID} \ --scope ${STORAGE_ACCOUNT_ID} \ --assignee-principal-type ServicePrincipal # MI is a type of Service Principal
    ```

4. Create the Federated Credentials with OIDC Issuer

    ```shell
    export K8S_NAMESPACE="default"
    export K8S_SERVICE_ACCOUNT="my-app-sa"

    az identity federated-credential create \
        --name "${MY_FEDERATED_IDENTITY_CRED_NAME}" \
        --identity-name "${USER_ASSIGNED_IDENTITY_NAME}" \
        --resource-group "${MY_RESOURCE_GROUP_NAME}" \
        --issuer "${AKS_OIDC_ISSUER}" \
        --subject system:serviceaccount:"${K8S_NAMESPACE}":"${K8S_SERVICE_ACCOUNT}" \
        --audience api://AzureADTokenExchange
    ```

#### Deploy Infrastructure

##### Authentication

Proper authentication is critical for enabling the Parc server, running within the Kubernetes
cluster, to communicate with the external Azure Storage Account. Without this setup, operations
relying on Azure Blob Storage, such as Blob RPCs, will fail. The core of this authentication
mechanism lies in two key configurations: The first defines the Kubernetes Service Account and links
it to the Azure User-Assigned Managed Identity via the `azure.workload.identity/client-id`
annotation. Subsequently, the deployment configuration assigns this specific Service Account to the
Parc server pods and crucially includes the `azure.workload.identity/use: 'true'` label, enabling
the Azure Workload Identity webhook to facilitate secure token acquisition for Azure access.

In your Kubernetes cluster you need a ServiceAccount:

##### Kubernetes Service Account Configuration

```yaml
apiVersion: v1
kind: ServiceAccount
metadata:
    name: <K8S_SERVICE_ACCOUNT> # Matches the service account name used in the federated credential subject
    namespace: <K8S_NAMESPACE> # Matches the namespace used in the federated credential subject
    annotations:
        azure.workload.identity/client-id: <USER_ASSIGNED_CLIENT_ID> # User-Assigned Managed ID Client ID
```

In the deployment you will also need the following:

##### Kubernetes Deployment Configuration

```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: parc-server
  namespace: <K8S_NAMESPACE>
  labels:
    app.kubernetes.io/name: parc-azure
spec:
  selector:
    matchLabels:
      app: parc-server
  template:
    metadata:
      labels:
        app: parc-server
        azure.workload.identity/use: 'true' # !IMPORTANT!
    spec:
      serviceAccountName: <K8S_SERVICE_ACCOUNT> # Matches the service account name used in the federated credential subject
      initContainers:
  ...
 containers:
  ...
```

Note: A reference for a helmchart deployment of Parc Azure Server is located in
`testing/parc/azure/azure-k8s-hermetic/helmchart/`

### Server Flags:

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->

| Flag | Meaning | Default | Required |
| :---- | :---- | :---- | :---- |
| `address` | The address of the Parc server | `0.0.0.0` | N |
| `port` | The port on which to listen for requests | `51337` | N |
| `verbose` | Server log verbosity | `false` | N |
| `parameters_file_path` | File path to a `.jsonl` file which contains the parameters returned through GetParameter([Example](?tab=t.0#heading=h.uhrq7lf7enl3)) | N/A | Y |
| `blob_chunk_size` | Size of retrieved blob range in bytes | `4MB` | N |
| `metrics_file` | Local file used as destination for server metrics | N/A | N |
| `otel_collector` | `host:port` for Otel collector | N/A | N |
| `use_workload_auth` | If true, uses [Workload ID](https://learn.microsoft.com/en-us/azure/aks/workload-identity-overview?tabs=dotnet) for Storage Account auth. Otherwise uses [Shared Key](https://learn.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key) authentication (intended for dev purposes) | `false` | N |
| `account_name` | Account name for Shared Key auth | devstoreaccount1(required if using Shared Key Authentication) | N |
| `account_key` | Account key for Shared Key auth | Uses dev credentials found [here](https://learn.microsoft.com/en-us/azure/storage/common/storage-use-emulator#authorize-with-shared-key-credentials) (required if using Shared Key Authentication) | N |
| `blob_http_endpoint` | The URL associated with the Storage Account([Example](?tab=t.0#heading=h.uhrq7lf7enl3)) | N/A | Y |
| `chunk_buffer_size` | Number of blob chunks to buffer for each GetBlob request | `3` | N |
| `certs_dir` | Path to directory container CA certificates. Required if `blob_http_endpoint` uses https | `/etc/ssl/certs` | N |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->
