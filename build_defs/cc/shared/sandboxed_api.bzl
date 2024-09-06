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
    # main as of 2024-02-06
    maybe(
        http_archive,
        name = "com_google_sandboxed_api",
        patch_args = ["-p1"],
        patches = [
            Label("//build_defs/cc/shared:sandboxed_api.patch"),
            Label("//build_defs/cc/shared:sandboxed_libunwind.patch"),
        ],
        sha256 = "6b4fa1bc9d20a57f9ca856c20646f5bbcc2706e15d2f417f2af9c2944eb90246",
        strip_prefix = "sandboxed-api-1bdcd66249da0494e976f812287e080610dd2ba3",
        urls = ["https://github.com/google/sandboxed-api/archive/1bdcd66249da0494e976f812287e080610dd2ba3.zip"],
    )
