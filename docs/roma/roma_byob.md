# Roma Bring-Your-Own-Binary

## Background

Roma Bring-Your-Own-Binary (BYOB) is a C++ library for untrusted binary execution in a
double-sandboxed environment.

Features:

-   High throughput, short duration executions.
-   Interface to accept a request and return response.
-   Support for the [Roma Host API](/docs/roma/host_api.md)

Currently, Roma only supports JS and WASM. With this project, Roma plans on expanding support to
execution of arbitrary binaries written in any language.

## How to use Roma BYOB?

### K/V, B&A and Inference

#### Prerequisites

1. To the `container_deps.bzl`, add the following:

    ```bazel
    load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", common_container_deps = "container_deps")
    ```

    and call `common_container_deps()` from `container_deps()`.

1. [For container image users] Add the following container_layers at an appropriate place:

    ```bazel
    load("@google_privacysandbox_servers_common//src/roma/byob/config:container.bzl", "roma_container_dir", "roma_container_root_dir")
    load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
    load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

    pkg_files(
      name = "gvisor_config_file",
      srcs = ["@google_privacysandbox_servers_common//src/roma/byob/container:container_config"],
      attributes = pkg_attributes(mode = "0777"),
    )

    pkg_tar(
        name = "gvisor_config_tar",
        srcs = [":gvisor_config_file"],
    )

    container_layer(
        name = "gvisor_config_layer",
        directory = "{}".format(roma_container_dir),
          tars = [
            ":gvisor_config_tar",
          ],
        )

    container_layer(
        name = "byob_server_container_layer",
        directory = "{roma_container_dir}/{root_dir}".format(roma_container_dir = roma_container_dir, root_dir = roma_container_root_dir),
        tars = [
          "@google_privacysandbox_servers_common//src/roma/byob/container:byob_server_container.tar",
        ],
    )
    ```

    Add the following layers to the container image running Roma BYOB:

    ```bazel
    "//path/to/layer:byob_server_container_layer",
    "//path/to/layer:gvisor_config_layer",
    ```

    Add the following tarball to your container image:

    ```bazel
    tars = [
          "@google_privacysandbox_servers_common//src/roma/byob/container:gvisor_tar",
      ],
    ```

    1. [For OCI image users] Add the following tars to the OCI image running Roma BYOB:

    ```bazel
    "@google_privacysandbox_servers_common//src/roma/byob/container:gvisor_tar",
    "@google_privacysandbox_servers_common//src/roma/byob/container:byob_server_container_with_dir.tar",
    ```

Links to images:

-   [K/V AWS](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/aws/data_server/BUILD.bazel#L63)
-   [K/V GCP](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/gcp/data_server/BUILD.bazel#L119)
-   [K/V Local](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/local/data_server/BUILD.bazel#L73)
-   [Bidding AWS](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/bidding_service/BUILD#L89)
-   [Bidding GCP](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/gcp/bidding_service/BUILD#L79)
-   [Auction AWS](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/auction_service/BUILD#L73)
-   [Auction GCP](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/gcp/auction_service/BUILD#L66)

#### Interface

Roma BYOB uses declarative interfaces.

Broadly, the steps for developing the interface are:

1. Define your API as a proto. Example [sample.proto](/src/roma/byob/udf/sample.proto).
1. Leverage this proto and generate Roma API code using
   [roma_byob_app_api_cc_library](/src/roma/tools/api_plugin/roma_api.bzl). Example
   [sample_roma_api](/src/roma/byob/udf/BUILD.bazel).
1. Add the target to your BUILD file that will be using the Roma API.
1. Use the API. Example [ByobSampleService](/src/roma/byob/benchmark/roma_byob_benchmark.cc).

### AdTechs

Currently, the team is working a performance optimization that will modify the interface for AdTechs
a bit up in the air. Once those changes are finalized, this documentation will be updated to reflect
this.

## FAQs

1. Which languages are supported? Currently, Roma BYOB supports C++, Go and other languages without
   special runtime needs.
1. The language we use is currently unsupported. Will it be supported in the future? It is one of
   our longer term goals to allow AdTechs to configure their own container. Once the details are
   finalized, the same will be announced.

## Questions?

For questions and bug reports, please reach out to <trusted-servers-infra-eng-team@google.com>.
