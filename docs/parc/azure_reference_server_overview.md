# Overview

This repository provides a reference implementation for a Parc server for
[Microsoft Azure](https://azure.microsoft.com/) in the `parc/azure/` directory. Instructions for
building the server binary can be found in [parc/azure/README.md](../../parc/azure/README.md). The
server artifacts are created using a Dockerfile-based build, see
[building the server](#building-the-server) below.

## Server implementation

The server is implemented using the
[gRPC C++ library](https://github.com/grpc/grpc/tree/master/src/cpp).
[Protobuf plugins for Bazel](https://rules-proto-grpc.com/en/stable/) generate C++ classes which can
be used to implement a server using [callback-based](https://grpc.io/docs/languages/cpp/callback/)
RPC handlers.

The Parc Azure server implements the `ParcService` implemented in the
[Parc API](https://github.com/privacysandbox/data-plane-shared-libraries/blob/main/docs/parc/api_overview.md).
The Parc Azure server mediates access to data from
[Azure Blob Storage](https://azure.microsoft.com/en-us/products/storage/blobs), and serves
parameters loaded from a local file. The API overview contains examples of successful requests and
responses. Error responses are returned in the form of
[gRPC error code](https://grpc.io/docs/guides/status-codes/) and message.

The main entrypoint for the server can be found in
[parc/azure/src/server/callback_server](../../parc/azure/src/server/cpp/callback_server.cc). See
[running the server](#running-the-server) for more detail.

This server is intended to be run on a variety of platforms. The current implementation uses either
[Shared Key authentication](https://learn.microsoft.com/en-us/rest/api/storageservices/authorize-with-shared-key)
or
[Workload Identity Authentication](https://learn.microsoft.com/en-us/azure/aks/workload-identity-overview?tabs=dotnet),
if running on
[Azure Kubernetes Service (AKS)](https://azure.microsoft.com/en-us/products/kubernetes-service). For
specific permissions and infrastructure required see
[suggested infrastructure configuration](#suggested-infrastructure-configuration).

## Suggested infrastructure configuration

The Parc server can be run as part of a Kubernetes cluster deployment in AKS or in a standalone VM.
See the `testing/parc/azure/azure-k8s-hermetic/helmchart` for an example workload specification if
using AKS.

The Parc Azure server reads data from two sources:

1. An
   [Azure Storage Account](https://learn.microsoft.com/en-us/azure/storage/common/storage-account-overview)
   for Blob data
1. A local file containing parameters in the form of key-value pairs. This local file can be
   provided either as a volume mount or a
   [ConfigMap](https://kubernetes.io/docs/tasks/configure-pod-container/configure-pod-configmap/)

In order for the Parc server to read from the target Storage Account, the Kubernetes cluster must be
configured to use
[Workload Identity](https://learn.microsoft.com/en-us/azure/aks/workload-identity-overview?tabs=dotnet)
authentication.

To access blobs from a standalone VM, the blob https endpoint of the storage account must be
reachable. For example, the storage account `mystorageaccount` would have a blob endpoint at
`https://mystorageaccount.blob.core.windows.net` which must be reachable from the VM.

For providing parameters, the server container looks for a local file at the path denoted by the
`--parameters_file_path` flag. The server is expecting a `jsonl` formatted file similar to the
following:

```json
{ "key": "parc-functional-test-param1", "value": "test-value1" }
{ "key": "parc-functional-test-param2", "value": "test-value2" }
{ "key": "parc-functional-test-unicode-param", "value": "hello_age_møller" }
```

**NOTE**: this parameters file is **required** and must be well-formed, or the server will not
start.

## Building the server

See the [parc/azure/README.md](../../parc/azure/README.md) for instructions on building the Parc
Azure server.

## Running the server

See [Azure Server Deployment Guide](azure_deployment_guide.md) for instructions on running the
server.
