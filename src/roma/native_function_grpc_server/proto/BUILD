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
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@rules_proto//proto:defs.bzl", "proto_library")
load(
    "//src/roma/tools/api_plugin:roma_api.bzl",
    "declare_roma_api",
    "js_proto_library",
    "roma_host_api_cc_library",
    "roma_integrator_docs",
    "roma_service_js_library",
    "roma_v8_app_api_cc_library",
    "roma_v8_sdk",
)

cc_library(
    name = "test_service_native_functions",
    srcs = ["test_service_native_functions.h"],
    deps = [
        "//src/roma/native_function_grpc_server/proto:multi_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:test_host_service_cc_proto",
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
    name = "test_host_service_proto",
    srcs = ["test_host_service.proto"],
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
    name = "test_host_service_cc_proto",
    visibility = [":grpc_proto_users_pkg"],
    deps = [
        ":test_host_service_proto",
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
    visibility = [
        ":grpc_proto_users_pkg",
        "//src/roma/benchmark:__subpackages__",
    ],
    deps = [
        ":callback_service_proto",
    ],
)

test_service_host_api = declare_roma_api(
    cc_protos = [":test_host_service_cc_proto"],
    proto_basename = "test_host_service",
    protos = [":test_host_service_proto"],
)

multi_service_host_api = declare_roma_api(
    cc_protos = [":multi_service_cc_proto"],
    proto_basename = "multi_service",
    protos = [":multi_service_proto"],
)

test_service_app_api = declare_roma_api(
    cc_protos = [":test_service_cc_proto"],
    host_apis = [
        test_service_host_api,
        multi_service_host_api,
    ],
    proto_basename = "test_service",
    protos = [":test_service_proto"],
)

js_proto_library(
    name = "test_service_app_js_proto_lib",
    roma_api = test_service_app_api,
)

js_proto_library(
    name = "test_service_host_js_proto_lib",
    roma_api = test_service_host_api,
)

js_proto_library(
    name = "multi_service_host_js_proto_lib",
    roma_api = multi_service_host_api,
)

roma_service_js_library(
    name = "test_service_js_library",
    roma_app_api = test_service_app_api,
    deps = [
        ":multi_service_host_js_proto_lib",
        ":test_service_app_js_proto_lib",
        ":test_service_host_js_proto_lib",
    ],
)

closure_js_binary(
    name = "test_service_js",
    compilation_level = "SIMPLE_OPTIMIZATIONS",
    debug = False,
    formatting = "PRETTY_PRINT",
    language = "ECMASCRIPT_NEXT",
    deps = [
        ":multi_service_host_js_proto_lib",
        ":test_service_app_js_proto_lib",
        ":test_service_host_js_proto_lib",
        ":test_service_js_library",
    ],
)

roma_host_api_cc_library(
    name = "test_service_roma_host_api",
    roma_host_api = test_service_host_api,
    tags = [
        "noasan",
        "notsan",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":test_service_native_functions",
    ],
)

roma_host_api_cc_library(
    name = "multi_service_roma_host_api",
    roma_host_api = multi_service_host_api,
    tags = [
        "noasan",
        "notsan",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":test_service_native_functions",
    ],
)

roma_v8_app_api_cc_library(
    name = "test_service_roma_app_api",
    js_library = ":test_service_js",
    roma_app_api = test_service_app_api,
    tags = [
        "noasan",
        "notsan",
    ],
    visibility = ["//visibility:public"],
)

roma_v8_sdk(
    name = "test_service_sdk",
    srcs = [":test_service_js"],
    app_api_cc_library = ":test_service_roma_app_api",
    js_library = ":test_service_js_library",
    roma_app_api = test_service_app_api,
    visibility = ["//visibility:public"],
)

roma_integrator_docs(
    name = "test_service_integrator_docs",
    app_api_cc_library = ":test_service_roma_app_api",
    host_api_cc_libraries = [
        ":test_service_roma_host_api",
        ":multi_service_roma_host_api",
    ],
)

cc_test(
    name = "test_service_cc_test",
    size = "small",
    srcs = [":test_service_roma_api_test.cc"],
    tags = [
        "noasan",
        "notsan",
    ],
    deps = [
        ":multi_service_roma_host_api",
        ":test_service_roma_app_api",
        ":test_service_roma_host_api",
        "//src/roma/config",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:lib",
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
    visibility = [
        ":grpc_proto_users_pkg",
        "//src/roma/config:__subpackages__",
    ],
    deps = [":callback_service_cc_proto"],
)

buf_lint_test(
    name = "test_service_proto_lint",
    size = "small",
    config = "//src:buf.yaml",
    targets = [":test_service_proto"],
)

buf_lint_test(
    name = "test_host_service_proto_lint",
    size = "small",
    config = "//src:buf.yaml",
    targets = [":test_host_service_proto"],
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
