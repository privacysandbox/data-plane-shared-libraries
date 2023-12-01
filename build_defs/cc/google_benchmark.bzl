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

def google_benchmark():
    maybe(
        http_archive,
        name = "google_benchmark",
        sha256 = "abfc22e33e3594d0edf8eaddaf4d84a2ffc491ad74b6a7edc6e7a608f690e691",
        strip_prefix = "benchmark-1.8.3",
        urls = [
            "https://github.com/google/benchmark/archive/refs/tags/v1.8.3.zip",
        ],
    )
