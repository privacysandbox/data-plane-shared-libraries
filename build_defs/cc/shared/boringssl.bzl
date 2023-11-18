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

def boringssl():
    maybe(
        # Commit from 2023-02-15.
        http_archive,
        name = "boringssl",
        strip_prefix = "boringssl-5c22014ca513807ed03c657e8ede076164663979",
        url = "https://github.com/google/boringssl/archive/5c22014ca513807ed03c657e8ede076164663979.zip",
        sha256 = "863fc670c456f30923740c1639305132fdfb9d1b25ba385a67ae3862ef12a8af",
    )
