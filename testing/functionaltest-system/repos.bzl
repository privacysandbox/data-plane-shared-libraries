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

"""Expose external repo dependencies for this WORKSPACE."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def repositories():
    """Register bazel repositories."""

    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "bc283cdfcd526a52c3201279cda4bc298652efa898b10b4db0837dc51652756f",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.7.1/bazel-skylib-1.7.1.tar.gz",
        ],
    )

    http_archive(
        name = "container_structure_test",
        sha256 = "4cbb382d3d7edc97129f79f83196c95e6006d9063d9edbb33a2311ba9372ad39",
        strip_prefix = "container-structure-test-1.19.3",
        urls = ["https://github.com/GoogleContainerTools/container-structure-test/archive/refs/tags/v1.19.3.zip"],
    )

    maybe(
        http_archive,
        name = "io_bazel_rules_go",
        sha256 = "f74c98d6df55217a36859c74b460e774abc0410a47cc100d822be34d5f990f16",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_go/releases/download/v0.47.1/rules_go-v0.47.1.zip",
            "https://github.com/bazelbuild/rules_go/releases/download/v0.47.1/rules_go-v0.47.1.zip",
        ],
    )

    http_archive(
        name = "bazel_gazelle",
        sha256 = "75df288c4b31c81eb50f51e2e14f4763cb7548daae126817247064637fd9ea62",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-gazelle/releases/download/v0.36.0/bazel-gazelle-v0.36.0.tar.gz",
            "https://github.com/bazelbuild/bazel-gazelle/releases/download/v0.36.0/bazel-gazelle-v0.36.0.tar.gz",
        ],
    )

    http_archive(
        name = "com_google_protobuf",
        sha256 = "44d5861127df4f94ae57e249bf494ebafba8dec514779fdd5304a9644119a7c8",
        strip_prefix = "protobuf-26.1",
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/refs/tags/v26.1.zip",
        ],
    )

    maybe(
        http_archive,
        name = "com_google_absl",
        # commit e83ef27 2024-11-06
        sha256 = "950869f55ffcfc316abd2213137de058664234ce6466514c8c80f7b5b30695ab",
        strip_prefix = "abseil-cpp-e83ef279682c46a0f8009a8f0727241693e96233",
        urls = ["https://github.com/abseil/abseil-cpp/archive/e83ef279682c46a0f8009a8f0727241693e96233.zip"],
    )

    maybe(
        http_archive,
        name = "rules_pkg",
        sha256 = "8f9ee2dc10c1ae514ee599a8b42ed99fa262b757058f65ad3c384289ff70c4b8",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
            "https://github.com/bazelbuild/rules_pkg/releases/download/0.9.1/rules_pkg-0.9.1.tar.gz",
        ],
    )

    http_archive(
        name = "zlib",
        build_file = Label("//:third_party/zlib.BUILD"),
        sha256 = "d14c38e313afc35a9a8760dadf26042f51ea0f5d154b0630a31da0540107fb98",
        strip_prefix = "zlib-1.2.13",
        urls = [
            "https://github.com/madler/zlib/releases/download/v1.2.13/zlib-1.2.13.tar.xz",
            "https://zlib.net/zlib-1.2.13.tar.xz",
        ],
    )

    ### rules_buf (https://docs.buf.build/build-systems/bazel)
    maybe(
        http_archive,
        name = "rules_buf",
        sha256 = "523a4e06f0746661e092d083757263a249fedca535bd6dd819a8c50de074731a",
        strip_prefix = "rules_buf-0.1.1",
        urls = ["https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.1.1.zip"],
    )

    maybe(
        # Commit from 2023-02-15.
        http_archive,
        name = "boringssl",
        sha256 = "863fc670c456f30923740c1639305132fdfb9d1b25ba385a67ae3862ef12a8af",
        strip_prefix = "boringssl-5c22014ca513807ed03c657e8ede076164663979",
        url = "https://github.com/google/boringssl/archive/5c22014ca513807ed03c657e8ede076164663979.zip",
    )

    maybe(
        http_archive,
        name = "aspect_bazel_lib",
        sha256 = "a8a92645e7298bbf538aa880131c6adb4cf6239bbd27230f077a00414d58e4ce",
        strip_prefix = "bazel-lib-2.7.2",
        url = "https://github.com/aspect-build/bazel-lib/releases/download/v2.7.2/bazel-lib-v2.7.2.tar.gz",
    )

def test_repositories():
    """Retrieve example protobuf files used for tests"""

    http_file(
        name = "grpc_example_helloworld_proto",
        sha256 = "6aff2c8e665b8fc0fdbe404a049016892c3de018177250d51d0e818669c9c3bb",
        url = "https://raw.githubusercontent.com/grpc/grpc/v1.52.2/examples/protos/helloworld.proto",
    )

    http_file(
        name = "grpc_example_route_guide_proto",
        sha256 = "5379accb8d9156b3a992f6d7a1633441f80ec849a8fbce22c1b263677942fbcf",
        url = "https://raw.githubusercontent.com/grpc/grpc/v1.52.2/examples/protos/route_guide.proto",
    )
