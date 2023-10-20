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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "v8_js_engine_test",
    size = "small",
    srcs = ["v8_js_engine_test.cc"],
    data = [
        "//scp/cc/roma/testing/cpp_wasi_dependency_example",
        "//scp/cc/roma/testing/cpp_wasm_string_in_string_out_example",
    ],
    tags = ["noasan"],  # TODO(b/290798808) - Fix ASAN errors in this test.
    deps = [
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "//scp/cc/roma/sandbox/js_engine/src/v8_engine:roma_v8_js_engine_lib",
        "//scp/cc/roma/wasm/test:roma_wasm_testing_lib",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "v8_isolate_visitor_function_binding_test",
    size = "small",
    srcs = ["v8_isolate_visitor_function_binding_test.cc"],
    tags = ["noasan"],  # TODO(b/290798808) - Fix ASAN errors in this test.
    deps = [
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "//scp/cc/roma/sandbox/js_engine/src/v8_engine:roma_v8_js_engine_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "snapshot_compilation_context_test",
    size = "small",
    srcs = ["snapshot_compilation_context_test.cc"],
    tags = ["noasan"],  # TODO(b/290798808) - Fix ASAN errors in this test.
    deps = [
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "//scp/cc/roma/sandbox/js_engine/src/v8_engine:roma_v8_js_engine_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
