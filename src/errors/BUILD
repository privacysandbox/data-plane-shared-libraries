# Copyright 2024 Google LLC
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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

cc_library(
    name = "retry",
    srcs = [
        "retry.cc",
    ],
    hdrs = [
        "retry.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//src/util/sleep:sleepfor",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/time",
        "@google_privacysandbox_servers_common//src/logger:request_context_logger",
        "@google_privacysandbox_servers_common//src/telemetry:tracing",
    ],
)

cc_test(
    name = "retry_test",
    size = "small",
    srcs = [
        "retry_test.cc",
    ],
    deps = [
        ":retry",
        "//src/util/sleep:sleepfor_mock",
        "@com_google_googletest//:gtest_main",
    ],
)
