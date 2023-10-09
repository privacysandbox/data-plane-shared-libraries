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
    name = "aws_private_key_service_factory_lib",
    srcs =
        [
            "aws_private_key_service_factory.cc",
            "aws_private_key_service_factory.h",
        ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/cpio/client_providers/auth_token_provider/src/aws:aws_auth_token_provider_lib",
        "//scp/cc/cpio/client_providers/instance_client_provider/src/aws:aws_instance_client_provider_lib",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/cpio/client_providers/private_key_client_provider/src:private_key_client_provider_lib",
        "//scp/cc/cpio/client_providers/private_key_fetcher_provider/src/aws:aws_private_key_fetcher_provider_lib",
        "//scp/cc/cpio/client_providers/role_credentials_provider/src/aws:aws_role_credentials_provider_lib",
        "//scp/cc/cpio/server/interface/private_key_service:private_key_service_interface_lib",
        "//scp/cc/cpio/server/src/component_factory:component_factory_lib",
        "//scp/cc/cpio/server/src/private_key_service:private_key_service_factory_lib",
    ],
)

cc_library(
    name = "nontee_aws_private_key_service_factory_lib",
    srcs =
        [
            "nontee_aws_private_key_service_factory.cc",
            "nontee_aws_private_key_service_factory.h",
        ],
    deps = [
        ":aws_private_key_service_factory_lib",
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/cpio/client_providers/kms_client_provider/src/aws:nontee_aws_kms_client_provider_lib",
    ],
)

cc_library(
    name = "tee_aws_private_key_service_factory_lib",
    srcs =
        [
            "tee_aws_private_key_service_factory.cc",
            "tee_aws_private_key_service_factory.h",
        ],
    deps = [
        ":aws_private_key_service_factory_lib",
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/cpio/client_providers/kms_client_provider/src/aws:tee_aws_kms_client_provider_lib",
    ],
)
