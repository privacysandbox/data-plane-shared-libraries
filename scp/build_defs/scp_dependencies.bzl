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

load("//scp/build_defs/cc/aws:aws_sdk_cpp_source_code_deps.bzl", "import_aws_sdk_cpp")
load("//scp/build_defs/cc/shared:bazel_rules_cpp.bzl", "bazel_rules_cpp")
load("//scp/build_defs/cc/shared:bazelisk.bzl", "bazelisk")
load("//scp/build_defs/cc/shared:boost.bzl", "boost")
load("//scp/build_defs/cc/shared:boringssl.bzl", "boringssl")
load("//scp/build_defs/cc/shared:cc_utils.bzl", "cc_utils")
load("//scp/build_defs/cc/shared:google_cloud_cpp.bzl", "import_google_cloud_cpp")
load("//scp/build_defs/cc/shared:gtest.bzl", "google_test")
load("//scp/build_defs/cc/shared:nghttp2.bzl", "nghttp2")
load("//scp/build_defs/shared:absl.bzl", "absl")
load("//scp/build_defs/shared:bazel_build_tools.bzl", "bazel_build_tools")
load("//scp/build_defs/shared:bazel_docker_rules.bzl", "bazel_docker_rules")
load("//scp/build_defs/shared:bazel_rules_pkg.bzl", "bazel_rules_pkg")
load("//scp/build_defs/shared:bazel_rules_proto.bzl", "bazel_rules_proto")
load("//scp/build_defs/shared:bazel_rules_python.bzl", "bazel_rules_python")
load("//scp/build_defs/shared:differential_privacy.bzl", "differential_privacy")
load("//scp/build_defs/shared:enclaves_kmstools.bzl", "enclaves_kmstools_libraries")
load("//scp/build_defs/shared:golang.bzl", "go_deps")
load("//scp/build_defs/shared:google_cloud_sdk.bzl", "google_cloud_sdk")
load("//scp/build_defs/shared:grpc.bzl", "grpc")
load("//scp/build_defs/shared:packer.bzl", "packer")
load("//scp/build_defs/shared:protobuf.bzl", "protobuf")
load("//scp/build_defs/shared:rpm.bzl", "rpm")
load("//scp/build_defs/shared:terraform.bzl", "terraform")

def scp_dependencies(protobuf_version, protobuf_repo_hash):
    absl()
    bazel_build_tools()
    bazel_docker_rules()
    bazel_rules_cpp()
    bazel_rules_pkg()
    bazel_rules_proto()
    bazel_rules_python()
    bazelisk()
    boost()
    boringssl()
    cc_utils()
    differential_privacy()
    enclaves_kmstools_libraries()
    go_deps()
    google_cloud_sdk()
    google_test()
    grpc()
    nghttp2()
    packer()
    protobuf(protobuf_version, protobuf_repo_hash)
    rpm()
    terraform()
    import_aws_sdk_cpp()
    import_google_cloud_cpp()
