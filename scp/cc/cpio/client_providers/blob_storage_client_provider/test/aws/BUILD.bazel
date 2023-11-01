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

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "aws_blob_storage_client_provider_test",
    size = "small",
    srcs = [
        "aws_blob_storage_client_provider_stream_test.cc",
        "aws_blob_storage_client_provider_test.cc",
        "aws_blob_storage_client_utils_test.cc",
        "mock_s3_client.h",
    ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/mock:core_async_executor_mock",
        "//scp/cc/core/async_executor/src/aws:core_aws_async_executor_lib",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/core/utils/src:core_utils",
        "//scp/cc/cpio/client_providers/blob_storage_client_provider/src/aws:aws_blob_storage_client_provider_lib",
        "//scp/cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@aws_sdk_cpp//:core",
        "@aws_sdk_cpp//:s3",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "test_aws_blob_storage_client_provider_lib",
    srcs = [
        "test_aws_blob_storage_client_provider.cc",
        "test_aws_blob_storage_client_provider.h",
        "//scp/cc/cpio/client_providers/blob_storage_client_provider/src/aws:aws_blob_storage_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//scp/cc/cpio/client_providers/blob_storage_client_provider/src/aws:aws_blob_storage_client_provider_lib",
        "//scp/cc/cpio/common/test/aws:test_aws_utils_lib",
        "//scp/cc/public/cpio/test/blob_storage_client:test_aws_blob_storage_client_options",
    ],
)
