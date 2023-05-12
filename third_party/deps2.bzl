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

"""Further initialization of shared control plane dependencies."""

load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@build_bazel_rules_swift//swift:repositories.bzl", "swift_rules_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
load("@control_plane_shared//build_defs/cc:sdk.bzl", scp_sdk_dependencies2 = "sdk_dependencies2")
load("@control_plane_shared//build_defs/cc:v8.bzl", "import_v8")
load("@control_plane_shared//build_defs/tink:tink_defs.bzl", "import_tink_git")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

GO_TOOLCHAINS_VERSION = "1.19.9"

def buf_dependencies():
    # rules_buf (https://docs.buf.build/build-systems/bazel)
    maybe(
        http_archive,
        name = "rules_buf",
        sha256 = "3fe244c9efa42a41edd83f63dee1b5570a1951a654030658b86bfaea6a268164",
        strip_prefix = "rules_buf-0.1.0",
        urls = ["https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.1.0.zip"],
    )

def quiche_dependencies():
    maybe(
        http_archive,
        name = "com_github_google_quiche",
        sha256 = "6f62d6d4bce6c81ed4493c1f53d4859b110e08efa7fdf58dc8c6bd232a6a9d84",
        strip_prefix = "quiche-c06013fca03cc95f662cb3b09ad582b0336258aa",
        urls = ["https://github.com/google/quiche/archive/c06013fca03cc95f662cb3b09ad582b0336258aa.tar.gz"],
    )
    maybe(
        http_archive,
        name = "com_google_quic_trace",
        # Last updated 2022-05-18
        sha256 = "079331de8c3cbf145a3b57adb3ad4e73d733ecfa84d3486e1c5a9eaeef286549",
        strip_prefix = "quic-trace-c7b993eb750e60c307e82f75763600d9c06a6de1",
        urls = ["https://github.com/google/quic-trace/archive/c7b993eb750e60c307e82f75763600d9c06a6de1.tar.gz"],
    )
    maybe(
        http_archive,
        name = "com_google_googleurl",
        sha256 = "a1bc96169d34dcc1406ffb750deef3bc8718bd1f9069a2878838e1bd905de989",
        urls = ["https://storage.googleapis.com/quiche-envoy-integration/googleurl_9cdb1f4d1a365ebdbcbf179dadf7f8aa5ee802e7.tar.gz"],
    )

def deps2(
        *,
        go_toolchains_version = GO_TOOLCHAINS_VERSION):
    grpc_deps()
    scp_sdk_dependencies2("@control_plane_shared")
    bazel_skylib_workspace()
    go_rules_dependencies()
    go_register_toolchains(version = go_toolchains_version)
    rules_pkg_dependencies()
    import_v8("@control_plane_shared")
    import_tink_git("@control_plane_shared")
    swift_rules_dependencies()
    quiche_dependencies()
    buf_dependencies()
    boost_deps()
