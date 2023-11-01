# Copyright 2023 Google LLC#
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

load("@rules_cc//cc:defs.bzl", "cc_binary")

# Need to be public visible in order to build SDK container in any other places.
package(default_visibility = ["//visibility:public"])

cc_binary(
    name = "crypto_service",
    srcs = ["crypto_service.cc"],
    linkopts = [
        # Tell LD to look in the working directory of the executable for SO
        # files to link
        "-Wl,-rpath,'$$ORIGIN'",
    ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/src:core_async_executor_lib",
        "//scp/cc/core/http2_client/src:http2_client_lib",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/network/src:core_network_lib",
        "//scp/cc/cpio/client_providers/crypto_client_provider/src:crypto_client_provider_lib",
        "//scp/cc/cpio/server/interface/crypto_service:crypto_service_interface_lib",
        "//scp/cc/cpio/server/src:service_utils_lib",
        "//scp/cc/public/cpio/interface/crypto_client:type_def",
        "//scp/cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_grpc",
        "//scp/cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "@com_github_grpc_grpc//:grpc++",
    ],
)
