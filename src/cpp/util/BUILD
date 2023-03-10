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

load("@bazel_skylib//lib:selects.bzl", "selects")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "duration",
    srcs = ["duration.cc"],
    hdrs = ["duration.h"],
    deps = [
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
    ],
)

cc_test(
    name = "duration_test",
    size = "small",
    srcs = ["duration_test.cc"],
    deps = [
        ":duration",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest_main",
    ],
)

selects.config_setting_group(
    name = "local_otel_otlp",
    match_all = [
        "//:local_instance",
        "//src/cpp/telemetry:local_otel_export_otlp",
    ],
)

selects.config_setting_group(
    name = "local_otel_ostream",
    match_all = [
        "//:local_instance",
        "//src/cpp/telemetry:local_otel_export_ostream",
    ],
)

local_defines = select({
    "//:aws_instance": ["INSTANCE=\\\"aws\\\""],
    "//:local_instance": ["INSTANCE=\\\"local\\\""],
    "//conditions:default": ["INSTANCE=\\\"unknown\\\""],
}) + select({
    "//:aws_platform": ["PLATFORM=\\\"aws\\\""],
    "//:local_platform": ["PLATFORM=\\\"local\\\""],
    "//conditions:default": ["PLATFORM=\\\"unknown\\\""],
}) + select({
    "//:aws_instance": ["OTEL_EXPORT=\\\"xray\\\""],
    ":local_otel_ostream": ["OTEL_EXPORT=\\\"ostream\\\""],
    ":local_otel_otlp": ["OTEL_EXPORT=\\\"otlp\\\""],
    "//conditions:default": ["OTEL_EXPORT=\\\"unknown\\\""],
})
