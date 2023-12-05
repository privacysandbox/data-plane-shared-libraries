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

def tink_dependencies():
    maybe(
        http_archive,
        name = "tink_cc",
        patch_args = ["-p2"],
        patches = [Label("//build_defs/tink:tink.patch")],
        sha256 = "ff272c968827ce06b262767934dc56ab520caa357a4747fc4a885b1cc711222f",
        strip_prefix = "tink-1.7.0/cc",
        urls = ["https://github.com/google/tink/archive/refs/tags/v1.7.0.zip"],
    )
