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

cc_test(
    name = "gcp_spanner_test",
    size = "large",
    srcs = [
        "gcp_nosql_database_client_provider_test.cc",
        "gcp_nosql_database_client_utils_test.cc",
    ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/async_executor/mock:core_async_executor_mock",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//scp/cc/cpio/client_providers/nosql_database_client_provider/src/gcp:gcp_nosql_database_provider_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:spanner",
        "@com_github_googleapis_google_cloud_cpp//:spanner_mocks",
        "@com_google_googletest//:gtest_main",
        "@nlohmann_json//:lib",
    ],
)

cc_library(
    name = "test_gcp_nosql_database_client_provider_lib",
    srcs = [
        "test_gcp_nosql_database_client_provider.cc",
        "test_gcp_nosql_database_client_provider.h",
        "//scp/cc/cpio/client_providers/nosql_database_client_provider/src/gcp:gcp_nosql_database_client_provider_srcs",
    ],
    copts = [
        "-DTEST_CPIO=1",
    ],
    deps = [
        "//scp/cc/cpio/client_providers/nosql_database_client_provider/src/gcp:gcp_nosql_database_provider_lib",
        "//scp/cc/public/cpio/test/blob_storage_client:test_gcp_blob_storage_client_options",
    ],
)
