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

"""Initialize the shared control plane repository."""

def scp_repositories():
    http_archive(
        name = "control_plane_shared",
        sha256 = "c0e43606dafd2e10206e9a10ac93179dec5f6e71e3e33c67fa808a0b0ab1b8f1",
        strip_prefix = "control-plane-shared-libraries-0.72.0",
        patch_args = ["-p1"],
        patches = [
            Label("//third_party:shared_control_plane.patch"),
        ],
        urls = [
            "https://github.com/privacysandbox/control-plane-shared-libraries/archive/refs/tags/v0.72.0.zip",
        ],
    )

def cpp_dependencies():
    scp_repositories()

    maybe(
        http_archive,
        name = "curl",
        build_file = Label("//third_party:curl.BUILD"),
        sha256 = "cdb38b72e36bc5d33d5b8810f8018ece1baa29a8f215b4495e495ded82bbf3c7",
        strip_prefix = "curl-7.88.1",
        urls = [
            "https://curl.haxx.se/download/curl-7.88.1.tar.gz",
            "https://github.com/curl/curl/releases/download/curl-7_88_1/curl-7.88.1.tar.gz",
        ],
    )
    maybe(
        http_archive,
        name = "jq",
        build_file = "//third_party:jq.BUILD",
        sha256 = "998c41babeb57b4304e65b4eb73094279b3ab1e63801b6b4bddd487ce009b39d",
        strip_prefix = "jq-1.4",
        urls = [
            "https://mirror.bazel.build/github.com/stedolan/jq/releases/download/jq-1.4/jq-1.4.tar.gz",
            "https://github.com/stedolan/jq/releases/download/jq-1.4/jq-1.4.tar.gz",
        ],
    )
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
        sha256 = "122fb6b712808ef43fbf80f75c52a21c9760683dae470154f02bddfc61135022",
        strip_prefix = "glog-0.6.0",
        urls = [
            "https://github.com/google/glog/archive/refs/tags/v0.6.0.zip",
        ],
    )
    maybe(
        http_archive,
        name = "com_google_googletest",
        sha256 = "ffa17fbc5953900994e2deec164bb8949879ea09b411e07f215bfbb1f87f4632",
        strip_prefix = "googletest-1.13.0",
        urls = [
            "https://github.com/google/googletest/archive/refs/tags/v1.13.0.zip",
        ],
    )
    maybe(
        http_archive,
        name = "io_opentelemetry_cpp",
        sha256 = "d333018f792b878d26989bc6913d1d21f82de0e82879ba98c599023742fb9521",
        strip_prefix = "opentelemetry-cpp-1.9.0",
        urls = [
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.9.0.zip",
        ],
    )
    maybe(
        http_archive,
        name = "brotli",
        sha256 = "84a9a68ada813a59db94d83ea10c54155f1d34399baf377842ff3ab9b3b3256e",
        strip_prefix = "brotli-3914999fcc1fda92e750ef9190aa6db9bf7bdb07",
        urls = ["https://github.com/google/brotli/archive/3914999fcc1fda92e750ef9190aa6db9bf7bdb07.zip"],  # 2022-11-17
    )
    maybe(
        http_archive,
        name = "build_bazel_rules_swift",
        sha256 = "bf2861de6bf75115288468f340b0c4609cc99cc1ccc7668f0f71adfd853eedb3",
        url = "https://github.com/bazelbuild/rules_swift/releases/download/1.7.1/rules_swift.1.7.1.tar.gz",
    )