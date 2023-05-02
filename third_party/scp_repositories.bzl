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
        sha256 = "6b746aa95337a08e0e9d84a432b92ff3b1e5973624c8d4cc0304e7a1760c4071",
        strip_prefix = "control-plane-shared-libraries-0.66.0",
        patches = [
            "@google_privacysandbox_servers_common//third_party:shared_control_plane.patch",
        ],
        urls = [
            "https://github.com/privacysandbox/control-plane-shared-libraries/archive/refs/tags/v0.66.0.zip",
        ],
    )
