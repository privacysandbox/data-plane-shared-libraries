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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def boost():
    maybe(
        http_archive,
        name = "com_github_nelhage_rules_boost",
        # latest as of 2024-01-06
        patch_args = ["-p1"],
        patches = [Label("//build_defs/cc/shared:rules_boost.patch")],
        sha256 = "eb852e3f9207debe5357bee329aaf2734ff36fec78dbcdb2f1c91e57f42105b8",
        strip_prefix = "rules_boost-e2b3a6894ded11b4dc029c4f23a3eab75ccb5c70",
        urls = ["https://github.com/nelhage/rules_boost/archive/e2b3a6894ded11b4dc029c4f23a3eab75ccb5c70.zip"],
    )
