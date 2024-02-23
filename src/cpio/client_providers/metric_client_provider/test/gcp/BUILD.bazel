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
    name = "gcp_metric_client_provider_test",
    size = "small",
    srcs = [
        "gcp_metric_client_provider_test.cc",
    ],
    deps = [
        "//scp/cc/core/async_executor/mock:core_async_executor_mock",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/cpio/client_providers/instance_client_provider/mock:instance_client_provider_mock",
        "//scp/cc/cpio/client_providers/metric_client_provider/mock/gcp:gcp_metric_client_provider_mock",
        "//scp/cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//scp/cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "//scp/cc/public/cpio/proto/metric_service/v1:metric_service_cc_proto",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_github_googleapis_google_cloud_cpp//:monitoring_mocks",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "gcp_metric_client_utils_test",
    size = "small",
    srcs = [
        "gcp_metric_client_utils_test.cc",
    ],
    deps = [
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/test/utils:utils_lib",
        "//scp/cc/cpio/client_providers/metric_client_provider/src:metric_client_provider_lib",
        "//scp/cc/cpio/client_providers/metric_client_provider/src/gcp:gcp_metric_client_provider_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_github_googleapis_google_cloud_cpp//:monitoring",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings",
        "@com_google_protobuf//:protobuf",
    ],
)
