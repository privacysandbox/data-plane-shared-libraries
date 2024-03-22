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

load("//build_defs/cc/shared:boost.bzl", "boost")
load("//build_defs/cc/shared:boringssl.bzl", "boringssl")
load("//build_defs/cc/shared:cc_utils.bzl", "cc_utils")
load("//build_defs/cc/shared:google_cloud_cpp.bzl", "import_google_cloud_cpp")
load("//build_defs/cc/shared:gtest.bzl", "google_test")
load("//build_defs/cc/shared:nghttp2.bzl", "nghttp2")
load("//build_defs/shared:golang.bzl", "go_deps")
load("//build_defs/shared:grpc.bzl", "grpc")
load("//build_defs/tink:tink_defs.bzl", "tink_dependencies")

def sdk_common():
    boost()
    boringssl()
    cc_utils()
    go_deps()
    google_test()
    grpc()
    nghttp2()
    import_google_cloud_cpp()
    tink_dependencies()
