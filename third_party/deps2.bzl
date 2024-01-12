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

load("@aspect_bazel_lib//lib:repositories.bzl", "aspect_bazel_lib_dependencies", "aspect_bazel_lib_register_toolchains")
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@build_bazel_rules_swift//swift:repositories.bzl", "swift_rules_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
load("@google_privacysandbox_servers_common//build_defs/cc:google_benchmark.bzl", "google_benchmark")
load("@google_privacysandbox_servers_common//build_defs/cc:sdk_source_code.bzl", scp_sdk_dependencies2 = "sdk_dependencies2")
load("@google_privacysandbox_servers_common//build_defs/cc:v8.bzl", "import_v8")
load("@google_privacysandbox_servers_common//build_defs/cc/shared:sandboxed_api.bzl", "sandboxed_api")
load("@google_privacysandbox_servers_common//build_defs/shared:rpm.bzl", "rpm")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")
load("@rules_oci//oci:dependencies.bzl", "rules_oci_dependencies")
load("@rules_oci//oci:repositories.bzl", "LATEST_CRANE_VERSION", "oci_register_toolchains")
load("@rules_pkg//pkg:deps.bzl", "rules_pkg_dependencies")
load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")
load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")
load("//third_party:emscripten_deps2.bzl", "emscripten_deps2")

GO_TOOLCHAINS_VERSION = "1.21.1"

def _buf_deps():
    # rules_buf (https://docs.buf.build/build-systems/bazel)
    maybe(
        http_archive,
        name = "rules_buf",
        sha256 = "523a4e06f0746661e092d083757263a249fedca535bd6dd819a8c50de074731a",
        strip_prefix = "rules_buf-0.1.1",
        urls = ["https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.1.1.zip"],
    )

def _proto_deps():
    maybe(
        http_archive,
        name = "rules_proto_grpc",
        sha256 = "9ba7299c5eb6ec45b6b9a0ceb9916d0ab96789ac8218269322f0124c0c0d24e2",
        strip_prefix = "rules_proto_grpc-4.5.0",
        urls = ["https://github.com/rules-proto-grpc/rules_proto_grpc/releases/download/4.5.0/rules_proto_grpc-4.5.0.tar.gz"],
    )

def _quiche_deps():
    maybe(
        http_archive,
        name = "com_github_google_quiche",
        patch_args = ["-p1"],
        patches = [Label("//third_party:quiche.patch")],
        sha256 = "563cbc483a006d4999e2e9b1114fec02cdc904fcdafa29721e4e6d816c8d648a",
        strip_prefix = "quiche-cc0614c8ab209e297f7b17ab3d04618fee327a4f",
        urls = ["https://github.com/google/quiche/archive/cc0614c8ab209e297f7b17ab3d04618fee327a4f.tar.gz"],
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
    aspect_bazel_lib_dependencies()
    aspect_bazel_lib_register_toolchains()
    go_rules_dependencies()
    go_register_toolchains(version = go_toolchains_version)
    rpm()
    grpc_deps()
    scp_sdk_dependencies2()
    bazel_skylib_workspace()
    gazelle_dependencies()
    rules_pkg_dependencies()
    import_v8()
    sandboxed_api()
    google_benchmark()
    swift_rules_dependencies()
    _quiche_deps()
    _proto_deps()
    _buf_deps()
    boost_deps()
    rules_rust_dependencies()
    rust_register_toolchains(
        edition = "2018",
        versions = [
            "1.74.0",
        ],
    )
    crate_universe_dependencies()
    rules_fuzzing_dependencies()
    emscripten_deps2()
    rules_oci_dependencies()
    oci_register_toolchains(
        name = "oci",
        crane_version = LATEST_CRANE_VERSION,
    )
