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
        # commit 248f0e4 2023-12-12
        name = "io_bazel_rules_closure",
        sha256 = "f5318f5c06bc02270852359812555cfda87b34e4002c855ba25a5a81ff551ac9",
        strip_prefix = "rules_closure-248f0e4a2bfd6063e88278be081d55715ecd298c",
        urls = [
            "https://github.com/bazelbuild/rules_closure/archive/248f0e4a2bfd6063e88278be081d55715ecd298c.zip",
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
        sha256 = "29b0f6b6d5714f212b8549cd0cb6fc531672630e41fb99d445421bc4d1bbb9cd",
        strip_prefix = "protobuf-21.12",
        urls = [
            "https://github.com/protocolbuffers/protobuf/releases/download/v21.12/protobuf-all-21.12.zip",
        ],
    )

    maybe(
        http_archive,
        name = "protocolbuffers_protobuf_javascript",
        patch_args = ["-p1"],
        patches = [Label("//third_party:protocolbuffers_protobuf_javascript.patch")],
        sha256 = "5657980a7475a8aaafa69ae9d156cce262aa0038c502168bd092c81c121fab9b",
        strip_prefix = "protobuf-javascript-3.21.2",
        repo_mapping = {
            "@com_google_protobuf": "@com_google_protobuf_for_closure",
        },
        urls = [
            "https://github.com/protocolbuffers/protobuf-javascript/archive/refs/tags/v3.21.2.zip",
        ],
    )
