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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = ["//scp/cc:scp_internal_pkg"])

TEST_SRCS = [
    "aws_dynamo_db_test.cc",
    "aws_dynamo_db_utils_test.cc",
]

cc_library(
    name = "test_aws_dynamo_db_lib",
    srcs = glob(
        [
            "**/*.cc",
            "**/*.h",
        ],
        exclude = TEST_SRCS,
    ),
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/src/aws:core_aws_async_executor_lib",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/nosql_database_provider/src/aws:core_nosql_database_provider_aws_lib",
        "@aws_sdk_cpp//:core",
    ],
)

cc_test(
    name = "aws_dynamo_db_test",
    size = "small",
    srcs =
        TEST_SRCS,
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/mock:core_async_executor_mock",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/leasable_lock/src:leasable_lock_lib",
        "//scp/cc/core/lease_manager/src:core_lease_manager_lib",
        "//scp/cc/core/nosql_database_provider/mock:nosql_database_provider_mock_lib",
        "//scp/cc/core/nosql_database_provider/src/aws:core_nosql_database_provider_aws_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@aws_sdk_cpp//:core",
        "@aws_sdk_cpp//:dynamodb",
        "@com_google_googletest//:gtest_main",
    ],
)
