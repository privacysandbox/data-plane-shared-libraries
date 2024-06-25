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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

cc_library(
    name = "interface",
    hdrs = ["interface.h"],
    visibility = [
        "//src/roma/config:__subpackages__",
        "//src/roma/roma_service:__subpackages__",
        "//src/roma/sandbox/js_engine/v8_engine:__subpackages__",
    ],
    deps = [
        "//src/roma/metadata_storage",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log:check",
    ],
)

cc_library(
    name = "request_handlers",
    hdrs = ["request_handlers.h"],
    visibility = ["//src/roma/roma_service:__subpackages__"],
    deps = [
        ":interface",
        "//src/roma/native_function_grpc_server/proto:logging_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:logging_service_grpc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log",
    ],
)

cc_library(
    name = "test_request_handlers",
    hdrs = ["test_request_handlers.h"],
    visibility = ["//src/roma/roma_service:__subpackages__"],
    deps = [
        ":interface",
        "//src/roma/native_function_grpc_server/proto:multi_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:multi_service_grpc_proto",
        "//src/roma/native_function_grpc_server/proto:test_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:test_service_grpc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "native_function_grpc_server",
    hdrs = ["native_function_grpc_server.h"],
    visibility = ["//src/roma/roma_service:__subpackages__"],
    deps = [
        ":interface",
        "//src/core/common/uuid",
        "//src/roma/metadata_storage",
        "@com_github_grpc_grpc//:grpc++",
    ],
)

cc_test(
    name = "native_function_grpc_server_test",
    size = "small",
    srcs = ["native_function_grpc_server_test.cc"],
    data = [
        ":grpc_client",
        ":grpc_logging_client",
        ":grpc_multi_client",
    ],
    deps = [
        ":native_function_grpc_server",
        ":request_handlers",
        ":test_request_handlers",
        "//src/roma/config",
        "//src/roma/metadata_storage",
        "//src/roma/native_function_grpc_server/proto:multi_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:multi_service_grpc_proto",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:scoped_mock_log",
        "@com_google_googletest//:gtest_main",
    ],
)

# Build and run with:
# builders/tools/bazel-debian build \
# src/roma/grpc:grpc_client && \
# ROMA_VLOG_LEVEL=0 bazel-bin/src/roma/native_function_grpc_server/grpc_client
cc_binary(
    name = "grpc_client",
    srcs = ["grpc_client.cc"],
    deps = [
        ":interface",
        "//src/roma/native_function_grpc_server/proto:test_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:test_service_grpc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

# Build and run with:
# builders/tools/bazel-debian build \
# src/roma/grpc:grpc_logging_client && \
# ROMA_VLOG_LEVEL=0 bazel-bin/src/roma/native_function_grpc_server/grpc_logging_client
cc_binary(
    name = "grpc_logging_client",
    srcs = ["grpc_logging_client.cc"],
    deps = [
        ":interface",
        "//src/roma/native_function_grpc_server/proto:logging_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:logging_service_grpc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

# Build and run with:
# builders/tools/bazel-debian build \
# src/roma/grpc:grpc_multi_client && \
# ROMA_VLOG_LEVEL=0 bazel-bin/src/roma/native_function_grpc_server/grpc_multi_client
cc_binary(
    name = "grpc_multi_client",
    srcs = ["grpc_multi_client.cc"],
    deps = [
        ":interface",
        "//src/roma/native_function_grpc_server/proto:multi_service_cc_proto",
        "//src/roma/native_function_grpc_server/proto:multi_service_grpc_proto",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/flags:usage",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:globals",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)
