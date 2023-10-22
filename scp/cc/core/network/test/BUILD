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

load("@com_github_grpc_grpc//bazel:grpc_build_system.bzl", "grpc_proto_library")
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//scp/cc:scp_internal_pkg"])

proto_library(
    name = "helloworld_proto",
    srcs = ["helloworld.proto"],
)

grpc_proto_library(
    name = "helloworld_grpc",
    srcs = ["helloworld.proto"],
)

cc_test(
    name = "grpc_server_test",
    size = "small",
    srcs = ["grpc_server_test.cc"],
    deps = [
        ":helloworld_grpc",
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/network/src:core_network_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "grpc_tag_manager_test",
    size = "small",
    srcs = ["grpc_tag_manager_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/network/src:core_network_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

buf_lint_test(
    name = "helloworld_proto_lint",
    size = "small",
    config = "//scp:buf.yaml",
    targets = [":helloworld_proto"],
)
