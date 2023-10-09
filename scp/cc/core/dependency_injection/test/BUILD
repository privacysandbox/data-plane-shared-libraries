# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//scp/cc:scp_internal_pkg"])

cc_test(
    name = "component_dependency_node_collection_test",
    size = "small",
    srcs = ["component_dependency_node_collection_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/dependency_injection/src:dependency_injection_lib",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "dependency_graph_test",
    size = "small",
    srcs = ["dependency_graph_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/dependency_injection/mock:mock_lib",
        "//scp/cc/core/dependency_injection/src:dependency_injection_lib",
        "//scp/cc/core/interface:logger_interface",
        "//scp/cc/core/logger/src:logger_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "dependency_injection_service_test",
    size = "large",
    srcs = ["dependency_injection_service_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/dependency_injection/mock:mock_lib",
        "//scp/cc/core/dependency_injection/src:dependency_injection_lib",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
