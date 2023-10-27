# Copyright 2022 Google LLC
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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "sandboxed_service_test",
    size = "large",
    srcs = ["sandboxed_service_test.cc"],
    data = [
        "//scp/cc/roma/testing/cpp_wasm_allocate_memory",
        "//scp/cc/roma/testing/cpp_wasm_string_in_string_out_example",
    ],
    tags = [
        "noasan",  # TODO(b/293876281) - Fix ASAN errors in this test.
        "notsan",  # TODO(b/307540489) - Fix TSAN errors in this test.
        "noubsan",  # TODO(b/308167161) - Fix UBSAN errors in this test.
    ],
    deps = [
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/roma/roma_service/src:roma_service_lib",
        "//scp/cc/roma/wasm/test:roma_wasm_testing_lib",
        "//src/cpp/util:duration",
        "@com_google_googletest//:gtest_main",
    ],
)
