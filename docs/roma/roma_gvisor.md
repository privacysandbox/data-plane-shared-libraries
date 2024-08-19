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

1. [For K/V only] To the `container_deps.bzl`, add the following:

    ```bazel
    load("@rules_oci//oci:pull.bzl", "oci_pull")

    def _oci_deps():
      images = {
        "runtime-debian-nondebug-nonroot": {
                "arch_hashes": {
                    # cc-debian11:nondebug-nonroot
                    # This image contains a minimal Linux, glibc runtime for "mostly-statically compiled" languages like Rust and D.
                    # https://github.com/GoogleContainerTools/distroless/blob/main/cc/README.md
                    "amd64": "5a9e854bab8498a61a66b2cfa4e76e009111d09cb23a353aaa8d926e29a653d9",
                    "arm64": "3122cd55375a0a9f32e56a18ccd07572aeed5682421432701a03c335ab79c650",
                },
                "registry": "gcr.io",
                "repository": "distroless/cc-debian11",
            },
      }
      [
            oci_pull(
                name = "{}-{}".format(img_name, arch),
                digest = "sha256:{}".format(hash),
                image = "{}/{}".format(image["registry"], image["repository"]),
            )
            for img_name, image in images.items()
            for arch, hash in image["arch_hashes"].items()
      ]
    ```

    and call `_oci_deps()` from `container_deps()`.

    B&A can skip this step as it pulls images from this repo (data-plane-shared-libraries).

1. Add the following container_layers at an appropriate place:

    ```bazel
    load("@google_privacysandbox_servers_common//src/roma/gvisor/config:container.bzl", "roma_container_dir", "roma_container_root_dir")

    pkg_tar(
        name = "gvisor_config_tar",
        srcs = ["@google_privacysandbox_servers_common//src/roma/gvisor/container:container_config"],
    )

    container_layer(
        name = "gvisor_config_layer",
        directory = "/{}".format(roma_container_dir),
          tars = [
            ":gvisor_config_tar",
          ],
        )

    container_layer(
        name = "gvisor_server_container_layer",
        directory = "/{roma_container_dir}/{root_dir}".format(roma_container_dir = roma_container_dir, root_dir = roma_container_root_dir),
        tars = [
          "@google_privacysandbox_servers_common//src/roma/gvisor/container:gvisor_server_container.tar",
        ],
    )
    ```

1. The layers defined should added to the relevant container image(s), for example:

    - [K/V AWS](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/aws/data_server/BUILD.bazel#L81)
    - [K/V GCP](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/gcp/data_server/BUILD.bazel#L140)
    - [K/V Local](https://github.com/privacysandbox/protected-auction-key-value-service/blob/5d586e0046e7b482e70c1b97bf322a923340bfab/production/packaging/local/data_server/BUILD.bazel#L91)
    - [Bidding AWS](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/bidding_service/BUILD#L112)
    - [Bidding GCP](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/gcp/bidding_service/BUILD#L94)
    - [Auction AWS](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/aws/auction_service/BUILD#L96)
    - [Auction GCP](https://github.com/privacysandbox/bidding-auction-servers/blob/c98a51c7dc11de92e9c8fb719242a033e620a1b4/production/packaging/gcp/auction_service/BUILD#L81)

    ```bazel
    ":gvisor_server_container_layer",
    ":gvisor_config_layer",
    ```

1. Add the following tarball to your container image:

    ```bazel
    tars = [
          "@google_privacysandbox_servers_common//src/roma/gvisor/container:gvisor_tar",
      ],
    ```

#### Interface

Roma gVisor uses declarative interfaces.

Broadly, the steps for developing the interface are:

1. Define your API as a proto. Example [sample.proto](/src/roma/gvisor/udf/sample.proto).
1. Leverage this proto and generate Roma API code using
   [roma_app_api_cc_library](/src/roma/tools/api_plugin/roma_api.bzl). Example
   [sample_roma_api](/src/roma/gvisor/udf/BUILD.bazel).
1. Add the target to your BUILD file that will be using the Roma API.
1. Use the API. Example [GvisorSampleService](/src/roma/gvisor/benchmark/roma_gvisor_benchmark.cc).

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
