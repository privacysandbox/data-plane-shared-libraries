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

def bazel_rules_closure():
    maybe(
        http_archive,
        name = "io_bazel_rules_closure",
        sha256 = "9498e57368efb82b985db1ed426a767cbf1ba0398fd7aed632fc3908654e1b1e",
        strip_prefix = "rules_closure-0.12.0",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_closure/archive/0.12.0.tar.gz",
            "https://github.com/bazelbuild/rules_closure/archive/0.12.0.tar.gz",
        ],
    )

    # This is currently needed for closure_js_proto_library,
    # since the server is using protobuf 3.23*., which no longer has javascript support.
    # rules_closure uses this version of protoc.
    # To make `closure_js_proto_library` work, we need to pass
    # closure_js_proto_library(
    #   ...
    #   protocbin = "@com_google_protobuf_for_closure//:protoc"
    # )
    maybe(
        http_archive,
        name = "com_google_protobuf_for_closure",
        sha256 = "387e2c559bb2c7c1bc3798c4e6cff015381a79b2758696afcbf8e88730b47389",
        strip_prefix = "protobuf-3.19.6",
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/refs/tags/v3.19.6.zip",
        ],
    )
