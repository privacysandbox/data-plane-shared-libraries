# CPIO

Provides a unified interface to talk to different cloud platforms. AWS and GCP are supported
currently.

# Build

## Prerequisites

This project needs to be built using [Bazel](https://bazel.build/install). To get reproducible
build, this project needs to be built inside container.

## Building the project

A flag is defined to choose cloud platform at building time: `//:platform`. Supported values are
"aws" and "gcp" currently. A flag is defined to choose running the code inside TEE or not:
`//src/public/cpio/interface:run_inside_tee`. Supported values are "True" and "False".

An example build command is as follows:

        bazel build --//:platform=aws --//src/public/cpio/interface:run_inside_tee=True src/public/cpio/...

## Running tests

        bazel test src/public/cpio/... && bazel test src/cpio/...

# Using CPIO

Before using any CPIO clients, CPIO needs to be initialized by calling Cpio::InitCpio. After all the
usage, CPIO needs to be cleaned up by calling Cpio::ShutdownCpio. In between, any clients can be
created and used following this pattern:

        client = ClientFactory::Create(options);
        client->Init();
        client->Run();
        # Use the client
        ...
        client->Stop();

# Layout

1. [src/public/cpio/interface](/src/public/cpio/interface) and
   [src/public/core/interface](/src/public/core/interface): interfaces and all other public visible
   targets provided to users of this project.
1. [src/public/cpio/mock](/src/public/cpio/mock): public visible targets to help with unit tests.
1. [src/public/cpio/test](/src/public/cpio/test): public visible targets to help with integration
   tests.
1. [src/public/cpio/examples](/src/public/cpio/examples): example codes for different clients.
1. [src/public/cpio/adapters](/src/public/cpio/adapters),
   [src/public/cpio/core](/src/public/cpio/core), [src/cpio](/src/cpio) and [src/core](/src/core):
   implementations. The targets there are not public visible.
1. [build_defs/cc](/build_defs/cc), [WORKSPACE](/WORKSPACE): external dependencies.
1. [src/public/cpio/utils](/src/public/cpio/utils): abstract some common functionalities above
   clients to help the customers to use the clients more conveniently.

# Clients

1. [BlobStorageClient](/src/public/cpio/interface/blob_storage_client)
1. [CryptoClient](/src/public/cpio/interface/crypto_client)
1. [InstanceClient](/src/public/cpio/interface/instance_client)
1. [KmsClient](/src/public/cpio/interface/kms_client)
1. [MetricClient](/src/public/cpio/interface/metric_client)
1. [ParameterClient](/src/public/cpio/interface/parameter_client)
1. [PrivateKeyClient](/src/public/cpio/interface/private_key_client)
1. [PublicKeyClient](/src/public/cpio/interface/public_key_client)

# Client helpers

There are several utility helpers above Clients to help the customers to use the clients more
conveniently. Currently we have:

1. [MetricAggregation](/src/public/cpio/utils/metric_aggregation): help to pre-aggregate metrics for
   high-qps traffic.
1. [SyncUtils](/src/public/cpio/utils/sync_utils): to help converting async calls to sync calls.
