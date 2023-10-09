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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//scp/cc:scp_internal_pkg"])

cc_library(
    name = "test_aws_private_key_service_factory_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/network/src:core_network_lib",
        "//scp/cc/cpio/client_providers/instance_client_provider/test/aws:test_aws_instance_client_provider_lib",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/cpio/client_providers/kms_client_provider/test/aws:test_aws_kms_client_provider_lib",
        "//scp/cc/cpio/client_providers/private_key_fetcher_provider/test/aws:test_aws_private_key_fetcher_provider_lib",
        "//scp/cc/cpio/client_providers/role_credentials_provider/test/aws:test_aws_role_credentials_provider_lib",
        "//scp/cc/cpio/server/interface/private_key_service:private_key_service_interface_lib",
        "//scp/cc/cpio/server/src:service_utils_lib",
        "//scp/cc/cpio/server/src/private_key_service/aws:nontee_aws_private_key_service_factory_lib",
        "//scp/cc/public/cpio/test/kms_client:test_aws_kms_client_options",
        "//scp/cc/public/cpio/test/private_key_client:test_aws_private_key_client_options",
    ],
)
