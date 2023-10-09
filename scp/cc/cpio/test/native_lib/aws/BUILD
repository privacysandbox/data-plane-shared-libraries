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
    name = "aws_cpio_lib_integration_test",
    size = "small",
    timeout = "long",
    srcs = ["aws_cpio_lib_integration_test.cc"],
    tags = ["manual"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/core/test/utils/aws_helper:aws_helper_lib",
        "//scp/cc/core/test/utils/docker_helper:docker_helper_lib",
        "//scp/cc/core/utils/src:core_utils",
        "//scp/cc/public/core/interface:execution_result",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "//scp/cc/public/cpio/interface:cpio_errors",
        "//scp/cc/public/cpio/test/blob_storage_client:test_aws_blob_storage_client",
        "//scp/cc/public/cpio/test/blob_storage_client:test_aws_blob_storage_client_options",
        "//scp/cc/public/cpio/test/global_cpio:test_lib_cpio",
        "//scp/cc/public/cpio/test/kms_client:test_aws_kms_client",
        "//scp/cc/public/cpio/test/kms_client:test_aws_kms_client_options",
        "//scp/cc/public/cpio/test/metric_client:test_aws_metric_client",
        "//scp/cc/public/cpio/test/parameter_client:test_aws_parameter_client",
        "//scp/cc/public/cpio/test/parameter_client:test_aws_parameter_client_options",
        "@com_google_googletest//:gtest_main",
    ],
)
