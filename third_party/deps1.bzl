# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Initialize the shared control plane dependencies."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@google_privacysandbox_servers_common//build_defs/cc:sdk_source_code.bzl", scp_sdk_dependencies = "sdk_dependencies")

def deps1():
    maybe(
        http_archive,
        name = "com_google_absl",
        sha256 = "497ebdc3a4885d9209b9bd416e8c3f71e7a1fb8af249f6c2a80b7cbeefcd7e21",
        strip_prefix = "abseil-cpp-20230802.1",
        urls = ["https://github.com/abseil/abseil-cpp/archive/refs/tags/20230802.1.zip"],
    )
    maybe(
        http_archive,
        name = "rules_rust",
        sha256 = "6357de5982dd32526e02278221bb8d6aa45717ba9bbacf43686b130aa2c72e1e",
        urls = ["https://github.com/bazelbuild/rules_rust/releases/download/0.30.0/rules_rust-v0.30.0.tar.gz"],
    )

    scp_sdk_dependencies()
