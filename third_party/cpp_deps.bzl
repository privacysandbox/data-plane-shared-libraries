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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

"""Initialize the shared control plane repository."""

EMSCRIPTEN_VER = "3.1.41"

def cpp_dependencies():
    maybe(
        http_archive,
        name = "rules_cc",
        sha256 = "2037875b9a4456dce4a79d112a8ae885bbc4aad968e6587dca6e64f3a0900cdf",
        strip_prefix = "rules_cc-0.0.9",
        urls = ["https://github.com/bazelbuild/rules_cc/releases/download/0.0.9/rules_cc-0.0.9.tar.gz"],
    )
    maybe(
        http_archive,
        name = "bazel_skylib",
        sha256 = "cd55a062e763b9349921f0f5db8c3933288dc8ba4f76dd9416aac68acee3cb94",
        urls = [
            "https://mirror.bazel.build/github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz",
            "https://github.com/bazelbuild/bazel-skylib/releases/download/1.5.0/bazel-skylib-1.5.0.tar.gz",
        ],
    )
    maybe(
        http_archive,
        name = "bazel_features",
        sha256 = "3aa581fef51598772c094b6b7b82004ce3fbb46d9fdc17edde410f901e4f2fea",
        strip_prefix = "bazel_features-1.13.0",
        urls = ["https://github.com/bazel-contrib/bazel_features/archive/refs/tags/v1.13.0.zip"],
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
    maybe(
        http_archive,
        name = "curl",
        build_file = Label("//third_party:curl.BUILD"),
        sha256 = "9c6db808160015f30f3c656c0dec125feb9dc00753596bf858a272b5dd8dc398",
        strip_prefix = "curl-8.6.0",
        urls = [
            "https://curl.haxx.se/download/curl-8.6.0.tar.gz",
            "https://github.com/curl/curl/releases/download/curl-8_6_0/curl-8.6.0.tar.gz",
        ],
    )
    http_file(
        name = "grpcurl_aarch64",
        url = "https://github.com/fullstorydev/grpcurl/releases/download/v1.8.9/grpcurl_1.8.9_linux_arm64.tar.gz",
        sha256 = "1303f4c1c6667f31b80efbe483875c749c94c8cb0d8b631bd64179f0b140714d",
    )
    http_file(
        name = "grpcurl_x86_64",
        url = "https://github.com/fullstorydev/grpcurl/releases/download/v1.8.9/grpcurl_1.8.9_linux_x86_64.tar.gz",
        sha256 = "a422d1e8ad854a305c0dd53f2f2053da242211d3d1810e7addb40a041e309516",
    )
    http_file(
        # ca_certificates for amd64 and aarch64 are the same.
        # Retrieved from https://github.com/GoogleContainerTools/distroless/blob/f55b2b343481f52ef3bde34c8a2f3631a40a36c1/debian_archives.bzl
        name = "ca_certificates_deb",
        urls = [
            "https://snapshot-cloudflare.debian.org/archive/debian/20240210T223313Z/pool/main/c/ca-certificates/ca-certificates_20230311_all.deb",
            "https://snapshot.debian.org/archive/debian/20240210T223313Z/pool/main/c/ca-certificates/ca-certificates_20230311_all.deb",
        ],
        sha256 = "5308b9bd88eebe2a48be3168cb3d87677aaec5da9c63ad0cf561a29b8219115c",
    )
    maybe(
        http_archive,
        name = "jq",
        build_file = Label("//third_party:jq.BUILD"),
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
        sha256 = "c61f4c6f820b04b920f35f84a3867cd44138bac4da21d21fbc00645c97e2051e",
        strip_prefix = "opentelemetry-cpp-1.9.1",
        urls = [
            "https://github.com/open-telemetry/opentelemetry-cpp/archive/refs/tags/v1.9.1.zip",
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
        urls = ["https://github.com/bazelbuild/rules_swift/releases/download/1.7.1/rules_swift.1.7.1.tar.gz"],
    )
    maybe(
        http_archive,
        name = "com_google_differential_privacy",
        sha256 = "161ae3676b7c75bb948a58c81bc982e5be4922f4ca7438237d8439857c42c640",
        strip_prefix = "differential-privacy-2.1.0",
        urls = ["https://github.com/google/differential-privacy/archive/refs/tags/v2.1.0.zip"],
    )
    maybe(
        http_archive,
        name = "com_google_cc_differential_privacy",
        patch_args = ["-p1"],
        patches = [Label("//third_party:differential_privacy.patch")],
        sha256 = "161ae3676b7c75bb948a58c81bc982e5be4922f4ca7438237d8439857c42c640",
        strip_prefix = "differential-privacy-2.1.0/cc",
        urls = ["https://github.com/google/differential-privacy/archive/refs/tags/v2.1.0.zip"],
    )
    maybe(
        http_archive,
        name = "emsdk",
        sha256 = "293eb67df598f44b23a07e247fc81107029eff7cd3b38d4ff531e32bf8a951eb",
        strip_prefix = "emsdk-{ver}/bazel".format(ver = EMSCRIPTEN_VER),
        urls = ["https://github.com/emscripten-core/emsdk/archive/refs/tags/{ver}.zip".format(ver = EMSCRIPTEN_VER)],
    )
    maybe(
        http_archive,
        name = "bazel_clang_tidy",
        sha256 = "352aeb57ad7ed53ff6e02344885de426421fb6fd7a3890b04d14768d759eb598",
        strip_prefix = "bazel_clang_tidy-4884c32e09c1ea9ac96b3f08c3004f3ac4c3fe39",
        urls = ["https://github.com/erenon/bazel_clang_tidy/archive/4884c32e09c1ea9ac96b3f08c3004f3ac4c3fe39.zip"],
    )
    maybe(
        http_archive,
        name = "com_github_google_flatbuffers",
        patch_args = ["-p1"],
        patches = [Label("//third_party:flatbuffers.patch")],
        sha256 = "e706f5eb6ca8f78e237bf3f7eccffa1c5ec9a96d3c1c938f08dc09aab1884528",
        strip_prefix = "flatbuffers-24.3.25",
        urls = ["https://github.com/google/flatbuffers/archive/refs/tags/v24.3.25.zip"],
    )
    _bazel_deps()
    _container_deps()
    _ts_js_deps()

def _bazel_deps():
    maybe(
        http_archive,
        name = "aspect_bazel_lib",
        sha256 = "688354ee6beeba7194243d73eb0992b9a12e8edeeeec5b6544f4b531a3112237",
        strip_prefix = "bazel-lib-2.8.1",
        urls = ["https://github.com/aspect-build/bazel-lib/releases/download/v2.8.1/bazel-lib-v2.8.1.tar.gz"],
    )

def _container_deps():
    maybe(
        http_archive,
        name = "rules_oci",
        patches = [
            Label("//third_party:rules_oci.patch"),
        ],
        sha256 = "d007e6c96eb62c88397b68f329e4ca56e0cfe31204a2c54b0cb17819f89f83c8",
        strip_prefix = "rules_oci-2.0.0",
        url = "https://github.com/bazel-contrib/rules_oci/releases/download/v2.0.0/rules_oci-v2.0.0.tar.gz",
    )
    maybe(
        http_archive,
        name = "container_structure_test",
        sha256 = "978db1ed0f802120fb0308b08b5c1e38ea81377944cc7a2fb727529815e4ed09",
        strip_prefix = "container-structure-test-1.17.0",
        urls = ["https://github.com/GoogleContainerTools/container-structure-test/archive/v1.17.0.zip"],
    )

def _ts_js_deps():
    maybe(
        http_archive,
        name = "aspect_rules_js",
        sha256 = "1b3efc640f6168bd2ff68d86b67e331e8f4747e70b36cb2e713f4ecc46f13e62",
        strip_prefix = "rules_js-1.42.3",
        urls = ["https://github.com/aspect-build/rules_js/archive/refs/tags/v1.42.3.zip"],
    )
    maybe(
        http_archive,
        name = "aspect_rules_ts",
        sha256 = "e1e3fd09f9cc3b12ea9eb0472920e5213fe595504cda516716d5707f41863567",
        strip_prefix = "rules_ts-2.4.2",
        urls = ["https://github.com/aspect-build/rules_ts/archive/refs/tags/v2.4.2.zip"],
    )
    maybe(
        http_archive,
        name = "aspect_rules_esbuild",
        sha256 = "f0f957410368a0ffc2ea923bceedf02e17b4b5d508553989e349f8c764d41bb8",
        strip_prefix = "rules_esbuild-0.20.1",
        urls = ["https://github.com/aspect-build/rules_esbuild/archive/refs/tags/v0.20.1.zip"],
    )
