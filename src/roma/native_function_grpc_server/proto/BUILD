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

load(
    "@com_google_googleapis_imports//:imports.bzl",
    "cc_grpc_library",
    "cc_proto_library",
)
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_proto//proto:defs.bzl", "proto_library")

proto_library(
    name = "test_service_proto",
    srcs = ["test_service.proto"],
)

proto_library(
    name = "logging_service_proto",
    srcs = ["logging_service.proto"],
)

cc_proto_library(
    name = "test_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":test_service_proto",
    ],
)

cc_proto_library(
    name = "logging_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":logging_service_proto",
    ],
)

cc_grpc_library(
    name = "test_service_grpc_proto",
    srcs = [":test_service_proto"],
    grpc_only = True,
    visibility = [":grpc_proto_users_pkg"],
    deps = [":test_service_cc_proto"],
)

cc_grpc_library(
    name = "logging_service_grpc_proto",
    srcs = [":logging_service_proto"],
    grpc_only = True,
    visibility = [":grpc_proto_users_pkg"],
    deps = [":logging_service_cc_proto"],
)

buf_lint_test(
    name = "test_service_proto_lint",
    size = "small",
    config = "//src:buf.yaml",
    targets = [":test_service_proto"],
)

buf_lint_test(
    name = "logging_service_proto_lint",
    size = "small",
    config = "//src:buf.yaml",
    targets = [":logging_service_proto"],
)

package_group(
    name = "grpc_proto_users_pkg",
    packages = [
        "//src/roma/native_function_grpc_server/...",
        "//src/roma/sandbox/js_engine/v8_engine/...",
    ],
)
