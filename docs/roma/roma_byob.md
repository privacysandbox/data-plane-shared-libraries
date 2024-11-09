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

## Code Map

<!-- prettier-ignore-start -->
<!-- markdownlint-disable line-length -->
| Directory                                | Summary                                                                                                                                                 |
| ---------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------- |
| [benchmark](/src/roma/byob/benchmark/)   | Contains [microbenchmarking target](https://github.com/google/benchmark) for a variety of UDFs. See [doc](/docs/roma/byob/Benchmarking.md) for details. |
| [config](/src/roma/byob/config/)         | Contains configuration options which can be used by integrators when initializing Roma BYOB.                                                            |
| [container](/src/roma/byob/container/)   | Contains code that runs inside gVisor's sandbox container.                                                                                              |
| [dispatcher](/src/roma/byob/dispatcher/) | Containers the business logic for handling requests.                                                                                                    |
| [example](/src/roma/byob/example/)       | Examples to help UDF developers.                                                                                                                        |
| [interface](/src/roma/byob/interface/)   | Interface for calling into Roma BYOB.                                                                                                                   |
| [test](/src/roma/byob/test/)             | Contains a myriad of tests for Roma BYOB libraries.                                                                                                     |
| [udf](/src/roma/byob/sample_udf/)        | UDFs used by [benchmarks](/src/roma/byob/benchmark/) and [tests](/src/roma/byob/test/).                                                                 |
| [utility](/src/roma/byob/utility/)       | Container utility functions used by Roma BYOB and also by Roma integrators.                                                                             |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->

## How to use Roma BYOB?

### K/V, B&A and Inference

#### Prerequisites

1. Install container deps:

    ```bazel
    load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", common_container_deps = "container_deps")

    common_container_deps()
    ```

1. Use the `get_user` macro, specifying either `root` or `nonroot`. For example:

1. [For OCI image users] Add create a target using the `roma_byob_image` macro, which supports all
   the attributes for `oci_image`. Attributes include:

    - `name`: Each image has a corresponding OCI tarball `{name}.tar`
    - `tars`: Image layers, which are added to the BYOB base image layers.
    - `repo_tags`: Tags for the resulting image.
    - `debug`: boolean specifying whether a debug base image is used.
    - `user`: Struct representing a {uid,gid}, use the `get_user()` macro, passing either `root` or
      `nonroot` as the arg.
    - `container_structure_test_configs`: Labels of config files for `container_structure_test`.
      Refer to docs at <https://github.com/GoogleContainerTools/container-structure-test>.
    - Other keyword attributes are passed through to `oci_image`.

    The following example will generate a tar file `your_image_name.tar`:

    ```bazel
    load("@google_privacysandbox_servers_common//src/roma/tools/api_plugin:roma_api.bzl", "roma_byob_image")
    load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", "get_user")

    roma_byob_image(
      name = "your_image_name",
      ...
      user = get_user("nonroot"),
    )
    ```

Links to images:

-   [K/V AWS](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/aws/data_server/BUILD.bazel#L63)
-   [K/V GCP](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/gcp/data_server/BUILD.bazel#L119)
-   [K/V Local](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/local/data_server/BUILD.bazel#L73)
-   [Bidding AWS](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/bidding_service/BUILD#L89)
-   [Bidding GCP](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/gcp/bidding_service/BUILD#L79)
-   [Auction AWS](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/auction_service/BUILD#L73)
-   [Auction GCP](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/gcp/auction_service/BUILD#L66)

Note: If you are having permission issues running your docker container, consider using
`--security-opt=seccomp=unconfined` and `--security-opt=apparmor=unconfined` flags to `docker run`.
If you are still having issues, try adding `--cap-add=CAP_SYS_ADMIN`.

#### Interface

Roma BYOB uses declarative interfaces.

Broadly, the steps for developing the interface are:

1. Define your API as a proto. Example [sample.proto](/src/roma/byob/sample_udf/sample.proto).
1. Leverage this proto and generate Roma API code using
   [roma_byob_app_api_cc_library](/src/roma/tools/api_plugin/roma_api.bzl). Example
   [sample_roma_api](/src/roma/byob/sample_udf/BUILD.bazel).
1. Add the target to your BUILD file that will be using the Roma API.
1. Use the API. Example [ByobSampleService](/src/roma/byob/benchmark/roma_byob_benchmark.cc).
