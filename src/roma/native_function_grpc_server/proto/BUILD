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
load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_binary")
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(
    "//src/roma/tools/api_plugin:roma_app_api.bzl",
    "declare_roma_app_api",
    "js_proto_library",
    "roma_client_cc_library",
    "roma_service_js_library",
)

cc_library(
    name = "test_service_native_functions",
    srcs = ["test_service_native_functions.h"],
    deps = [
        "//src/roma/native_function_grpc_server/proto:test_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:test_service_grpc_proto",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

proto_library(
    name = "test_service_proto",
    srcs = ["test_service.proto"],
    deps = [
        "//apis/privacysandbox/apis/roma/app_api/v1:options_proto",
    ],
)

proto_library(
    name = "multi_service_proto",
    srcs = ["multi_service.proto"],
    deps = [
        "//apis/privacysandbox/apis/roma/app_api/v1:options_proto",
    ],
)

proto_library(
    name = "logging_service_proto",
    srcs = ["logging_service.proto"],
)

proto_library(
    name = "callback_service_proto",
    srcs = ["callback_service.proto"],
)

cc_proto_library(
    name = "test_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":test_service_proto",
    ],
)

cc_proto_library(
    name = "multi_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":multi_service_proto",
    ],
)

cc_proto_library(
    name = "logging_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":logging_service_proto",
    ],
)

cc_proto_library(
    name = "callback_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":callback_service_proto",
    ],
)

test_service_api = declare_roma_app_api(
    cc_protos = [":test_service_cc_proto"],
    proto_basename = "test_service",
    protos = [":test_service_proto"],
)

js_proto_library(
    name = "test_service_js_proto_lib",
    roma_app_api = test_service_api,
)

roma_service_js_library(
    name = "test_service_js_library",
    roma_app_api = test_service_api,
    deps = [":test_service_js_proto_lib"],
)

closure_js_binary(
    name = "test_service_js",
    compilation_level = "SIMPLE_OPTIMIZATIONS",
    debug = False,
    formatting = "PRETTY_PRINT",
    language = "ECMASCRIPT_NEXT",
    deps = [
        ":test_service_js_library",
        ":test_service_js_proto_lib",
    ],
)

roma_client_cc_library(
    name = "test_service_roma_api",
    js_library = ":test_service_js",
    roma_app_api = test_service_api,
    tags = [
        "noasan",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":test_service_native_functions",
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
    name = "multi_service_grpc_proto",
    srcs = [":multi_service_proto"],
    grpc_only = True,
    visibility = [":grpc_proto_users_pkg"],
    deps = [":multi_service_cc_proto"],
)

cc_grpc_library(
    name = "logging_service_grpc_proto",
    srcs = [":logging_service_proto"],
    grpc_only = True,
    visibility = [":grpc_proto_users_pkg"],
    deps = [":logging_service_cc_proto"],
)

cc_grpc_library(
    name = "callback_service_grpc_proto",
    srcs = [":callback_service_proto"],
    grpc_only = True,
    visibility = [":grpc_proto_users_pkg"],
    deps = [":callback_service_cc_proto"],
)

buf_lint_test(
    name = "test_service_proto_lint",
    size = "small",
    config = "//src:buf.yaml",
    targets = [":test_service_proto"],
)

buf_lint_test(
    name = "multi_service_proto_lint",
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

buf_lint_test(
    name = "callback_service_proto_lint",
    size = "small",
    config = "//src:buf.yaml",
    targets = [":callback_service_proto"],
)

package_group(
    name = "grpc_proto_users_pkg",
    packages = [
        "//src/roma/native_function_grpc_server/...",
        "//src/roma/roma_service/...",
        "//src/roma/sandbox/js_engine/v8_engine/...",
    ],
)
