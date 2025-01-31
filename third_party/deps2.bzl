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

load(
    "@aspect_bazel_lib//lib:repositories.bzl",
    "aspect_bazel_lib_dependencies",
    "aspect_bazel_lib_register_toolchains",
    "register_copy_directory_toolchains",
    "register_copy_to_directory_toolchains",
    "register_coreutils_toolchains",
    "register_jq_toolchains",
)
load("@bazel_gazelle//:deps.bzl", "gazelle_dependencies")
load("@bazel_skylib//:workspace.bzl", "bazel_skylib_workspace")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@build_bazel_rules_swift//swift:repositories.bzl", "swift_rules_dependencies")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
load("@container_structure_test//:repositories.bzl", "container_structure_test_register_toolchain")
load("@depend_on_what_you_use//:setup_step_1.bzl", dwyu_setup_step_1 = "setup_step_1")
load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")
load("@emsdk//:toolchains.bzl", "register_emscripten_toolchains")
load("@io_bazel_rules_go//go:deps.bzl", "go_register_toolchains", "go_rules_dependencies")
load("@rules_fuzzing//fuzzing:repositories.bzl", "rules_fuzzing_dependencies")
load("@rules_graalvm//graalvm:repositories.bzl", "graalvm_repository")
load("@rules_graalvm//graalvm:workspace.bzl", "register_graalvm_toolchains", "rules_graalvm_repositories")
load("@rules_nodejs//nodejs:repositories.bzl", "DEFAULT_NODE_VERSION", "nodejs_register_toolchains")
load("@rules_oci//oci:dependencies.bzl", "rules_oci_dependencies")
load("@rules_oci//oci:repositories.bzl", "oci_register_toolchains")
load("@rules_pkg//pkg:deps.bzl", "rules_pkg_dependencies")
load("@rules_rust//crate_universe:repositories.bzl", "crate_universe_dependencies")
load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")
load("//build_defs/cc:google_benchmark.bzl", "google_benchmark")
load("//build_defs/cc:sdk_source_code.bzl", scp_sdk_dependencies2 = "sdk_dependencies2")
load("//build_defs/cc:v8.bzl", "import_v8")
load("//build_defs/cc/shared:sandboxed_api.bzl", "sandboxed_api")
load("//build_defs/shared:rpm.bzl", "rpm")
load("//third_party:cpp_deps.bzl", "EMSCRIPTEN_VER")

GO_TOOLCHAINS_VERSION = "1.21.1"
RUST_TOOLCHAINS_EDITION = "2021"
RUST_TOOLCHAINS_VERSIONS = [
    "1.78.0",
]

def _buf_deps():
    # rules_buf (https://docs.buf.build/build-systems/bazel)
    maybe(
        http_archive,
        name = "rules_buf",
        integrity = "sha256-Hr64Q/CaYr0E3ptAjEOgdZd1yc+cBjp7OG1wzuf3DIs=",
        strip_prefix = "rules_buf-0.3.0",
        urls = [
            "https://github.com/bufbuild/rules_buf/archive/refs/tags/v0.3.0.zip",
        ],
    )

def _proto_deps():
    maybe(
        http_archive,
        name = "rules_proto_grpc",
        sha256 = "a53cea895b9e870cdcfe5e50a1c61d8aa837c1d30b5886b210f0eb3e4709e4bc",
        strip_prefix = "rules_proto_grpc-4.6.0",
        urls = [
            "https://github.com/rules-proto-grpc/rules_proto_grpc/archive/refs/tags/4.6.0.zip",
        ],
    )

def _quiche_deps():
    maybe(
        http_archive,
        name = "com_github_google_quiche",
        patch_args = ["-p1"],
        patches = [Label("//third_party:quiche.patch")],
        sha256 = "2ffbbccd545a21277c47f95d9c40fb12ecc9acd1aba5c0d5aa9a2ae712be8450",
        strip_prefix = "quiche-c2c9d880e3a09956d3b6ad81aeb05b7e89766529",
        urls = ["https://github.com/google/quiche/archive/c2c9d880e3a09956d3b6ad81aeb05b7e89766529.tar.gz"],
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
        patch_args = ["-p1"],
        patches = [Label("//third_party:googleurl.patch")],
        # sha256 is unstable for this url
        urls = ["https://quiche.googlesource.com/googleurl/+archive/9cdb1f4d1a365ebdbcbf179dadf7f8aa5ee802e7.tar.gz"],
    )

def _nodejs_deps():
    nodejs_register_toolchains(
        name = "nodejs",
        node_version = DEFAULT_NODE_VERSION,
    )

def _emscripten_deps():
    emsdk_emscripten_deps(emscripten_version = EMSCRIPTEN_VER)
    register_emscripten_toolchains()

def _graalvm_deps():
    graalvm_repository(
        name = "graalvm",
        distribution = "ce",
        java_version = "20",
        version = "20.0.2",
    )
    rules_graalvm_repositories()
    register_graalvm_toolchains()

def deps2(
        *,
        go_toolchains_version = GO_TOOLCHAINS_VERSION):
    bazel_skylib_workspace()
    aspect_bazel_lib_dependencies()
    aspect_bazel_lib_register_toolchains()
    register_coreutils_toolchains()
    register_copy_directory_toolchains()
    register_copy_to_directory_toolchains()
    register_jq_toolchains()
    go_rules_dependencies()
    go_register_toolchains(version = go_toolchains_version)
    rpm()
    grpc_deps()
    scp_sdk_dependencies2()
    gazelle_dependencies()
    rules_pkg_dependencies()
    import_v8()
    sandboxed_api()
    google_benchmark()
    swift_rules_dependencies()
    _quiche_deps()
    _proto_deps()
    _buf_deps()
    _graalvm_deps()
    boost_deps()
    rules_rust_dependencies()
    rust_register_toolchains(
        edition = RUST_TOOLCHAINS_EDITION,
        versions = RUST_TOOLCHAINS_VERSIONS,
    )
    crate_universe_dependencies()
    rules_fuzzing_dependencies()
    _nodejs_deps()
    _emscripten_deps()
    rules_oci_dependencies()
    oci_register_toolchains(
        name = "oci",
    )
    container_structure_test_register_toolchain(name = "cst")
    dwyu_setup_step_1()
