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
    # main as of 12-07-2023
    maybe(
        http_archive,
        name = "com_google_sandboxed_api",
        patch_args = ["-p1"],
        patches = [
            Label("//build_defs/cc/shared:sandboxed_api.patch"),
            Label("//build_defs/cc/shared:sandboxed_libunwind.patch"),
        ],
        sha256 = "e9cd4fbc9f3b05fdd12e608f23acce0cadcef51a9a3b3ec0e7f6c98f7a4b24c0",
        strip_prefix = "sandboxed-api-19d8f4729a4af6c9f7966f43c3f2f5b7807aceec",
        urls = ["https://github.com/google/sandboxed-api/archive/19d8f4729a4af6c9f7966f43c3f2f5b7807aceec.zip"],
    )
