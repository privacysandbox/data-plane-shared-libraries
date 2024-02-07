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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "azure_auth_token_provider_lib",
    srcs = [
        "//scp/cc/cpio/client_providers/auth_token_provider/src/azure:azure_auth_token_provider_srcs",
    ],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/utils/src:core_utils",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/public/cpio/interface:cpio_errors",
        "//scp/cc/public/cpio/interface:type_def",
        "@com_google_absl//absl/functional:bind_front",
        "@com_google_absl//absl/strings",
        "@nlohmann_json//:lib",
    ],
)

exports_files([
    "azure_auth_token_provider.h",
    "azure_auth_token_provider.cc",
    "error_codes.h",
])

filegroup(
    name = "azure_auth_token_provider_srcs",
    srcs = [
        ":error_codes.h",
        ":azure_auth_token_provider.cc",
        ":azure_auth_token_provider.h",
    ],
)
