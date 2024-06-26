# Roma gVisor

## Background

Roma gVisor is a C++ library for untrusted binary execution in a double-sandboxed environment.

Features:

-   High throughput, short duration executions.
-   Interface to accept a request and return response.
-   Support for the [Roma Host API](/docs/roma/host_api.md)

Currently, Roma only supports JS and WASM. With this project, Roma plans on expanding support to
execution of arbitrary binaries written in any language.

## How to use Roma gVisor?

### K/V, B&A and Inference

#### Prerequisites

1. The specified gVisor targets should be added as layers to the relevant container image(s), for
   example
   [K/V](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/auction_service/BUILD#L73),
   [Auction](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/auction_service/BUILD#L73),
   [Bidding](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/bidding_service/BUILD#L113).

Targets to be added:

```shell
@google_privacysandbox_servers_common////src/roma/gvisor/container:gvisor_tar
@google_privacysandbox_servers_common////src/roma/gvisor/container:gvisor_server_container.tar
```

#### Interface

Roma gVisor uses declarative interfaces.

Broadly, the steps for developing the interface are:

1. Define your API as a proto. Example [kv.proto](/src/roma/gvisor/udf/kv.proto).
1. Leverage this proto and generate Roma API code using
   [roma_app_api_cc_library](/src/roma/tools/api_plugin/roma_api.bzl). Example
   [kv_roma_api](/src/roma/gvisor/udf/BUILD.bazel).
1. Add the target to your BUILD file that will be using the Roma API.
1. Use the API. Example
   [GvisorKeyValueService](/src/roma/gvisor/benchmark/roma_gvisor_benchmark.cc).

### AdTechs

Currently, the team is working a performance optimization that will modify the interface for AdTechs
a bit up in the air. Once those changes are finalized, this documentation will be updated to reflect
this.

## FAQs

1. Which languages are supported? Currently, Roma gVisor supports C++, Go and other languages
   without special runtime needs.
1. The language we use is currently unsupported. Will it be supported in the future? It is one of
   our longer term goals to allow AdTechs to configure their own container. Once the details are
   finalized, the same will be announced.

## Questions?

For questions and bug reports, please reach out to <trusted-servers-infra-eng-team@google.com>.
