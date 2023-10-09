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

package(default_visibility = ["//scp/cc:scp_internal_pkg"])

cc_test(
    name = "operation_dispatcher_test",
    size = "small",
    srcs = ["operation_dispatcher_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/mock:core_async_executor_mock",
        "//scp/cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//scp/cc/core/interface:type_def_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "retry_strategy_test",
    size = "small",
    srcs = ["retry_strategy_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/common/operation_dispatcher/src:operation_dispatcher_lib",
        "//scp/cc/core/interface:type_def_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "@com_google_googletest//:gtest_main",
    ],
)
