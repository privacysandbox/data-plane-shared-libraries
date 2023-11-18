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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# TODO: Build these libraries from source

def enclaves_kmstools_libraries():
    maybe(
        http_file,
        name = "kmstool_enclave_cli",
        downloaded_file_path = "kmstool_enclave_cli",
        executable = True,
        sha256 = "39ac7b55e30df69f963f8519686cd9e1ac3b815dd1f4cc85a35582bbc0fa6126",
        urls = ["https://storage.googleapis.com/scp-dependencies/aws/2023-03-27/kmstool_enclave_cli"],
    )
