# BlobStorageClient

Responsible for interacting with the Job service (Get/Put/Update).

# Build

## Building the client

    bazel build cc/public/cpio/interface/job_client/...

## Running tests

    bazel test cc/public/cpio/adapters/job_client/... && bazel test cc/cpio/client_providers/job_client_provider/...
