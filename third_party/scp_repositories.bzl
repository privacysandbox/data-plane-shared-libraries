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

"""Initialize the shared control plane repository."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def scp_repositories():
    http_archive(
        name = "control_plane_shared",
        sha256 = "d78f5b488a0f3f0f288230e20b381ef8ee7a3180e2de0fad159aa7709f138dae",
        strip_prefix = "control-plane-shared-libraries-0.64.0",
        patches = [
            "@google_privacysandbox_servers_common//third_party:shared_control_plane.patch",
        ],
        urls = [
            "https://github.com/privacysandbox/control-plane-shared-libraries/archive/refs/tags/v0.64.0.zip",
        ],
    )
