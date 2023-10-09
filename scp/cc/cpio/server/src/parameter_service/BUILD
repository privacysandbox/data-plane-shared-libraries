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
    name = "parameter_service",
    srcs = ["parameter_service.cc"],
    copts =
        select(
            {
                "//scp/cc/cpio/server/interface:aws_server": [
                    "-DAWS_SERVER=1",
                ],
                "//scp/cc/cpio/server/interface:gcp_server": [
                    "-DGCP_SERVER=1",
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
    deps = select(
        {
            "//scp/cc/cpio/server/interface:aws_server": [
                "//scp/cc/cpio/server/src/parameter_service/aws:aws_parameter_service_factory_lib",
            ],
            "//scp/cc/cpio/server/interface:gcp_server": [
                "//scp/cc/cpio/server/src/parameter_service/gcp:gcp_parameter_service_factory_lib",
            ],
            "//scp/cc/cpio/server/interface:test_aws_server": [
                "//scp/cc/cpio/server/src/parameter_service/test_aws:test_aws_parameter_service_factory_lib",
            ],
            "//scp/cc/cpio/server/interface:test_gcp_server": [
                "//scp/cc/cpio/server/src/parameter_service/test_gcp:test_gcp_parameter_service_factory_lib",
            ],
        },
        no_match_error = "Please build for AWS or GCP",
    ) + [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/network/src:core_network_lib",
        "//scp/cc/cpio/server/interface/parameter_service:parameter_service_interface_lib",
        "//scp/cc/cpio/server/src:service_utils_lib",
        "//scp/cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_grpc",
        "//scp/cc/public/cpio/proto/parameter_service/v1:parameter_service_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_library(
    name = "parameter_service_factory_lib",
    srcs = [
        "parameter_service_factory.cc",
        "parameter_service_factory.h",
    ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/cpio/server/interface/instance_service:instance_service_interface_lib",
        "//scp/cc/cpio/server/interface/parameter_service:parameter_service_interface_lib",
    ],
)
