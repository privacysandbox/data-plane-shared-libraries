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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

def scp_repositories():
    """Entry point for shared control plane repository and dependencies."""

    # Boost
    # latest as of 2022-06-09
    _RULES_BOOST_COMMIT = "789a047e61c0292c3b989514f5ca18a9945b0029"

    git_repository(
        name = "com_github_nelhage_rules_boost",
        commit = _RULES_BOOST_COMMIT,
        patch_args = ["-p1"],
        patches = ["//third_party:rules_boost.patch"],
        remote = "https://github.com/nelhage/rules_boost.git",
        shallow_since = "1652895814 -0700",
    )

    http_archive(
        name = "control_plane_shared",
        strip_prefix = "control-plane-shared-libraries-0.64.0",
        urls = [
            "https://github.com/privacysandbox/control-plane-shared-libraries/archive/refs/tags/v0.64.0.zip",
        ],
    )
