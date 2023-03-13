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

"""Expose dependencies for this WORKSPACE."""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def quiche_dependencies():
    maybe(
        http_archive,
        name = "zlib",
        build_file = "//third_party:zlib.BUILD",
        sha256 = "91844808532e5ce316b3c010929493c0244f3d37593afd6de04f71821d5136d9",
        strip_prefix = "zlib-1.2.12",
        urls = ["https://mirror.bazel.build/zlib.net/zlib-1.2.12.tar.gz"],
    )

    maybe(
        http_archive,
        name = "com_github_google_quiche",
        urls = ["https://github.com/google/quiche/archive/c06013fca03cc95f662cb3b09ad582b0336258aa.tar.gz"],
        strip_prefix = "quiche-c06013fca03cc95f662cb3b09ad582b0336258aa",
    )

    maybe(
        http_archive,
        name = "com_google_quic_trace",
        sha256 = "079331de8c3cbf145a3b57adb3ad4e73d733ecfa84d3486e1c5a9eaeef286549",  # Last updated 2022-05-18
        strip_prefix = "quic-trace-c7b993eb750e60c307e82f75763600d9c06a6de1",
        urls = ["https://github.com/google/quic-trace/archive/c7b993eb750e60c307e82f75763600d9c06a6de1.tar.gz"],
    )

    maybe(
        http_archive,
        name = "com_google_googleurl",
        sha256 = "a1bc96169d34dcc1406ffb750deef3bc8718bd1f9069a2878838e1bd905de989",
        urls = ["https://storage.googleapis.com/quiche-envoy-integration/googleurl_9cdb1f4d1a365ebdbcbf179dadf7f8aa5ee802e7.tar.gz"],
    )

def cpp_dependencies():
    maybe(
        ### Abseil
        http_archive,
        name = "com_google_absl",
        sha256 = "dcf71b9cba8dc0ca9940c4b316a0c796be8fab42b070bb6b7cab62b48f0e66c4",  # SHARED_ABSL_SHA
        strip_prefix = "abseil-cpp-20211102.0",
        urls = [
            "https://github.com/abseil/abseil-cpp/archive/refs/tags/20211102.0.tar.gz",
        ],
    )
    maybe(
        http_archive,
        name = "boringssl",
        sha256 = "0cd64ecff9e5f757988b84b7685e968775de08ea9157656d0b9fee0fa62d67ec",
        strip_prefix = "boringssl-c2837229f381f5fcd8894f0cca792a94b557ac52",
        urls = ["https://github.com/google/boringssl/archive/c2837229f381f5fcd8894f0cca792a94b557ac52.tar.gz"],
    )

    ### glog
    maybe(
        http_archive,
        name = "com_github_gflags_gflags",
        sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
        strip_prefix = "gflags-2.2.2",
        urls = ["https://github.com/gflags/gflags/archive/v2.2.2.tar.gz"],
    )
    maybe(
        http_archive,
        name = "com_github_google_glog",
        sha256 = "21bc744fb7f2fa701ee8db339ded7dce4f975d0d55837a97be7d46e8382dea5a",
        strip_prefix = "glog-0.5.0",
        urls = ["https://github.com/google/glog/archive/v0.5.0.zip"],
    )

    ### googletest
    maybe(
        http_archive,
        name = "com_google_googletest",
        strip_prefix = "googletest-e2239ee6043f73722e7aa812a459f54a28552929",
        urls = ["https://github.com/google/googletest/archive/e2239ee6043f73722e7aa812a459f54a28552929.zip"],
    )

    quiche_dependencies()
