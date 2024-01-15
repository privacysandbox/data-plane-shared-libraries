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

def import_aws_nitro_enclaves_sdk():
    """
    Import AWS Nitro Enclaves SDK
    """
    maybe(
        http_archive,
        name = "json_c",
        build_file = Label("//build_defs/cc/aws:json_c.BUILD"),
        sha256 = "471e9eb1dad4fd2e4fec571d8415993e66a89f23a5b052f1ba11b54db90252de",
        strip_prefix = "json-c-json-c-0.17-20230812",
        urls = [
            "https://github.com/json-c/json-c/archive/refs/tags/json-c-0.17-20230812.zip",
        ],
    )

    maybe(
        http_archive,
        name = "aws-nitro-enclaves-nsm-api",
        build_file = Label("//build_defs/cc/aws:aws_nitro_enclaves_nsm_api.BUILD"),
        sha256 = "8150bb1e9e757f24ff35b19c10b924e2d96ed2a81f98efe05048c50e2e0804e6",
        strip_prefix = "aws-nitro-enclaves-nsm-api-0.4.0",
        urls = ["https://github.com/aws/aws-nitro-enclaves-nsm-api/archive/refs/tags/v0.4.0.zip"],
    )
