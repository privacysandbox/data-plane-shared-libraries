# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

# Need to be public visible in order to build SDK container in any other places.
package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "private_key_service",
    srcs = ["private_key_service.cc"],
    copts =
        select(
            {
                "//scp/cc/cpio/server/interface:aws_server_inside_tee": [
                    "-DAWS_SERVER_INSIDE_TEE=1",
                ],
                "//scp/cc/cpio/server/interface:aws_server_outside_tee": [
                    "-DAWS_SERVER_OUTSIDE_TEE=1",
                ],
                "//scp/cc/cpio/server/interface:gcp_server_inside_tee": [
                    "-DGCP_SERVER_INSIDE_TEE=1",
                ],
                "//scp/cc/cpio/server/interface:gcp_server_outside_tee": [
                    "-DGCP_SERVER_OUTSIDE_TEE=1",
                ],
                "//scp/cc/cpio/server/interface:test_aws_server": [
                    "-DTEST_AWS_SERVER=1",
                ],
                "//scp/cc/cpio/server/interface:test_gcp_server": [
                    "-DTEST_GCP_SERVER=1",
                ],
            },
            no_match_error = "Please build for AWS or GCP",
        ),
    linkopts = [
        # Tell LD to look in the working directory of the executable for SO
        # files to link
        "-Wl,-rpath,'$$ORIGIN'",
    ],
    deps = select({
        "//scp/cc/cpio/server/interface:aws_server_inside_tee": [
            "//scp/cc/cpio/server/src/private_key_service/aws:tee_aws_private_key_service_factory_lib",
        ],
        "//scp/cc/cpio/server/interface:aws_server_outside_tee": [
            "//scp/cc/cpio/server/src/private_key_service/aws:nontee_aws_private_key_service_factory_lib",
        ],
        "//scp/cc/cpio/server/interface:gcp_server_inside_tee": [
            "//scp/cc/cpio/server/src/private_key_service/gcp:tee_gcp_private_key_service_factory_lib",
        ],
        "//scp/cc/cpio/server/interface:gcp_server_outside_tee": [
            "//scp/cc/cpio/server/src/private_key_service/gcp:nontee_gcp_private_key_service_factory_lib",
        ],
        "//scp/cc/cpio/server/interface:test_aws_server": [
            "//scp/cc/cpio/server/src/private_key_service/test_aws:test_aws_private_key_service_factory_lib",
        ],
        "//scp/cc/cpio/server/interface:test_gcp_server": [
            "//scp/cc/cpio/server/src/private_key_service/gcp:nontee_gcp_private_key_service_factory_lib",
        ],
    }) + [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/network/src:core_network_lib",
        "//scp/cc/cpio/server/interface/private_key_service:private_key_service_interface_lib",
        "//scp/cc/cpio/server/src:service_utils_lib",
        "//scp/cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_grpc",
        "//scp/cc/public/cpio/proto/private_key_service/v1:private_key_service_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_library(
    name = "private_key_service_factory_lib",
    srcs = [
        "error_codes.h",
        "private_key_service_factory.cc",
        "private_key_service_factory.h",
    ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/src:core_async_executor_lib",
        "//scp/cc/core/curl_client/src:http1_curl_client_lib",
        "//scp/cc/core/http2_client/src:http2_client_lib",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/network/src:core_network_lib",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_lib",
        "//scp/cc/cpio/server/interface/component_factory:component_factory_interface",
        "//scp/cc/cpio/server/interface/private_key_service:private_key_service_interface_lib",
        "//scp/cc/cpio/server/src:service_utils_lib",
        "//scp/cc/public/cpio/interface/private_key_client:type_def",
    ],
)
