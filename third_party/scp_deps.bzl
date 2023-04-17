# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Initialize the shared control plane dependencies."""

load("@control_plane_shared//build_defs/cc:sdk.bzl", scp_sdk_dependencies = "sdk_dependencies")
load("@control_plane_shared//build_defs/cc:v8.bzl", "import_v8")
load("@control_plane_shared//build_defs/tink:tink_defs.bzl", "import_tink_git")

################
# SCP SDK Dependencies Rules
################

# Declare explicit protobuf version and hash, to override any implicit dependencies.
PROTOBUF_CORE_VERSION = "3.19.4"

PROTOBUF_SHA_256 = "3bd7828aa5af4b13b99c191e8b1e884ebfa9ad371b0ce264605d347f135d2568"

def scp_deps():
    scp_sdk_dependencies(
        PROTOBUF_CORE_VERSION,
        PROTOBUF_SHA_256,
        "@control_plane_shared",
    )
    import_v8("@control_plane_shared")
    import_tink_git("@control_plane_shared")
