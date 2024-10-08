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
| [host](/src/roma/byob/host/)             | Host Callback API logic.                                                                                                                                |
| [interface](/src/roma/byob/interface/)   | Interface for calling into Roma BYOB.                                                                                                                   |
| [test](/src/roma/byob/test/)             | Contains a myriad of tests for Roma BYOB libraries.                                                                                                     |
| [udf](/src/roma/byob/sample_udf/)        | UDFs used by [benchmarks](/src/roma/byob/benchmark/) and [tests](/src/roma/byob/test/).                                                                 |
| [utility](/src/roma/byob/utility/)       | Container utility functions used by Roma BYOB and also by Roma integrators.                                                                             |
<!-- markdownlint-enable line-length -->
<!-- prettier-ignore-end -->

## How to use Roma BYOB?

### K/V, B&A and Inference

#### Prerequisites

1. To the `container_deps.bzl`, add the following:

    ```bazel
    load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", common_container_deps = "container_deps")
    ```

    and call `common_container_deps()` from `container_deps()`.

1. Define a variable called user. If you are running Roma BYOB as the user `root`, define:

    ```bazel
    load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", "get_user")
    user = get_user("root")
    ```

    If you are running Roma BYOB as the user `nonroot`, define:

    ```bazel
    load("@google_privacysandbox_servers_common//third_party:container_deps.bzl", "get_user")
    user= get_user("nonroot")
    ```

1. [For container image users] Add the following container_layers at an appropriate place:

    ```bazel
    load("@google_privacysandbox_servers_common//src/roma/byob/config:container.bzl", "roma_container_dir", "roma_container_root_dir")
    load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
    load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

    pkg_files(
      name = "gvisor_config_file",
      srcs = ["@google_privacysandbox_servers_common//src/roma/byob/container:container_config"],
      attributes = pkg_attributes(mode = "0600"),
    )

    pkg_tar(
      name = "gvisor_config_tar",
      srcs = [":gvisor_config_file"],
      owner = "{}.{}".format(
        user.uid,
        user.gid,
      ),
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
        "@google_privacysandbox_servers_common//src/roma/byob/container:byob_server_container_{user}.tar".format(user = user.user),
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
      "@google_privacysandbox_servers_common//src/roma/byob/container:gvisor_tar_{user}".format(user = user.user),
      "@google_privacysandbox_servers_common//src/roma/byob/container:var_run_runsc_tar_{user}".format(user = user.user),
    ],
    ```

    1. [For OCI image users] Add the following tars to the OCI image running Roma BYOB:

    ```bazel
    "@google_privacysandbox_servers_common//src/roma/byob/container:gvisor_tar_{user}".format(user.user),
    "@google_privacysandbox_servers_common//src/roma/byob/container:var_run_runsc_tar_{user}".format(user.user)",
    "@google_privacysandbox_servers_common//src/roma/byob/container:byob_server_container_with_dir_{user}.tar".format(user.user),
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
