# Copyright 2023 Google LLC
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

def sandboxed_api():
    # main as of 11-17-2023
    maybe(
        http_archive,
        name = "com_google_sandboxed_api",
        patch_args = ["-p1"],
        patches = [
            Label("//build_defs/cc/shared:sandboxed_api.patch"),
            Label("//build_defs/cc/shared:sandboxed_libunwind.patch"),
        ],
        sha256 = "ba8943fcb40bc4c5c5135f7df125fc3bf6a4a4b5bf15a0a56edf421700557479",
        strip_prefix = "sandboxed-api-a0ba1c520f0bb72e8d9a8bf17580c074e666960e",
        urls = ["https://github.com/google/sandboxed-api/archive/a0ba1c520f0bb72e8d9a8bf17580c074e666960e.zip"],
    )
