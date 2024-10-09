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

def import_google_cloud_cpp():
    # Load the googleapis dependency for gcloud.
    maybe(
        http_archive,
        name = "com_google_googleapis",
        build_file = Label("//build_defs/cc/shared/build_targets:googleapis.BUILD"),
        sha256 = "8e85caaee7e1238ff2dbdb6807ee3fbf44a41d712614a6fa61fa6df6ed8bb20b",
        strip_prefix = "googleapis-cd747534b861c84ea7577e17cccb30b19a24b467",
        urls = [
            "https://github.com/googleapis/googleapis/archive/cd747534b861c84ea7577e17cccb30b19a24b467.tar.gz",
        ],
    )
    maybe(
        http_archive,
        name = "com_github_googleapis_google_cloud_cpp",
        patch_args = ["-p1"],
        patches = [Label("//build_defs/cc/shared:google_cloud_cpp.patch")],
        repo_mapping = {
            "@com_github_curl_curl": "@curl",
            "@com_github_nlohmann_json": "@nlohmann_json",
        },
        sha256 = "bfc7e31496abe0520dd9e83fb1f7029b82d935f62e86cec89412af8ada156109",
        strip_prefix = "google-cloud-cpp-2.17.0",
        urls = [
            "https://github.com/googleapis/google-cloud-cpp/archive/refs/tags/v2.17.0.zip",
        ],
    )
