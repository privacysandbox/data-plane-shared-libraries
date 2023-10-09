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
    name = "gcp_parameter_service_factory_lib",
    srcs = glob(
        [
            "*.cc",
            "*.h",
        ],
    ),
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/cpio/client_providers/interface:cpio_client_providers_interface_lib",
        "//scp/cc/cpio/client_providers/parameter_client_provider/src/gcp:gcp_parameter_client_provider_lib",
        "//scp/cc/cpio/server/src/instance_service/gcp:gcp_instance_service_factory_lib",
        "//scp/cc/cpio/server/src/parameter_service:parameter_service_factory_lib",
        "//scp/cc/public/cpio/interface/parameter_client:type_def",
    ],
)
