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
    name = "crypto_client_test",
    size = "small",
    timeout = "long",
    srcs = ["crypto_client_test.cc"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/cpio/client_providers/crypto_client_provider/src:crypto_client_provider_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "//scp/cc/public/cpio/adapters/crypto_client/mock:crypto_client_mock",
        "//scp/cc/public/cpio/core/mock:lib_cpio_mock",
        "//scp/cc/public/cpio/interface:cpio_errors",
        "//scp/cc/public/cpio/interface:type_def",
        "//scp/cc/public/cpio/interface/crypto_client",
        "//scp/cc/public/cpio/interface/crypto_client:type_def",
        "//scp/cc/public/cpio/proto/crypto_service/v1:crypto_service_cc_proto",
        "@com_google_googletest//:gtest_main",
    ],
)
