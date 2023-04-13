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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@com_github_nelhage_rules_boost//:boost/boost.bzl", "boost_deps")
load("@control_plane_shared//build_defs/tink:tink_defs.bzl", "import_tink_git")

def scp_deps():
    boost_deps()

    import_tink_git()

    maybe(
        http_archive,
        name = "rules_foreign_cc",
        sha256 = "6041f1374ff32ba711564374ad8e007aef77f71561a7ce784123b9b4b88614fc",
        strip_prefix = "rules_foreign_cc-0.8.0",
        url = "https://github.com/bazelbuild/rules_foreign_cc/archive/0.8.0.tar.gz",
    )

    maybe(
        http_archive,
        name = "com_github_nghttp2_nghttp2",
        build_file = "@control_plane_shared//build_defs/cc/build_targets:nghttp2.BUILD",
        patch_args = ["-p1"],
        patches = ["@control_plane_shared//build_defs/cc/build_targets:nghttp2.patch"],
        sha256 = "62f50f0e9fc479e48b34e1526df8dd2e94136de4c426b7680048181606832b7c",
        strip_prefix = "nghttp2-1.47.0",
        url = "https://github.com/nghttp2/nghttp2/releases/download/v1.47.0/nghttp2-1.47.0.tar.gz",
    )

    # Used by control_plane_shared//cc/core/common/concurrent_map/src:concurrent_map_lib.
    git_repository(
        name = "oneTBB",
        # Commits on Apr 18, 2022
        commit = "9d2a3477ce276d437bf34b1582781e5b11f9b37a",
        remote = "https://github.com/oneapi-src/oneTBB.git",
        shallow_since = "1648820995 +0300",
    )

    git_repository(
        name = "v8",
        # tag 11.1.277.9
        commit = "3e5cb8693c290fa0f60eb92aad84d09e68507678",
        remote = "https://chromium.googlesource.com/v8/v8.git",
        shallow_since = "1677006717 +0000",
        patch_args = ["-p1"],
        patches = ["@control_plane_shared//build_defs/cc:v8.patch"],
    )

    http_archive(
        name = "curl",
        build_file = "//third_party:curl.BUILD",
        sha256 = "3dfdd39ba95e18847965cd3051ea6d22586609d9011d91df7bc5521288987a82",
        strip_prefix = "curl-7.86.0",
        urls = [
            "https://github.com/curl/curl/releases/download/curl-7_86_0/curl-7.86.0.tar.gz",
        ],
    )

    http_archive(
        name = "libxml2",
        build_file = "@//third_party:libxml2.BUILD",
        patch_args = ["-p1"],
        patches = ["@//third_party:libxml2.patch"],
        sha256 = "c8d6681e38c56f172892c85ddc0852e1fd4b53b4209e7f4ebf17f7e2eae71d92",
        strip_prefix = "libxml2-2.9.12",
        url = "http://xmlsoft.org/sources/libxml2-2.9.12.tar.gz",
    )

    maybe(
        new_git_repository,
        name = "nlohmann_json",
        build_file = "@//third_party:nlohmann.BUILD",
        # Commits on Apr 6, 2022
        commit = "15fa6a342af7b51cb51a22599026e01f1d81957b",
        remote = "https://github.com/nlohmann/json.git",
    )

    http_archive(
        name = "com_google_brotli",
        build_file = "@//third_party:brotli.BUILD",
        sha256 = "73a89a4a5ad295eed881795f2767ee9f7542946011f4b30385bcf2caef899df3",
        strip_prefix = "brotli-6d03dfbedda1615c4cba1211f8d81735575209c8",
        urls = ["https://github.com/google/brotli/archive/6d03dfbedda1615c4cba1211f8d81735575209c8.zip"],  # master(2022-11-08)
    )

    http_archive(
        name = "rapidjson",
        build_file = "@control_plane_shared//build_defs/cc/build_targets:rapidjson.BUILD",
        sha256 = "30bd2c428216e50400d493b38ca33a25efb1dd65f79dfc614ab0c957a3ac2c28",
        strip_prefix = "rapidjson-418331e99f859f00bdc8306f69eba67e8693c55e",
        urls = [
            "https://github.com/miloyip/rapidjson/archive/418331e99f859f00bdc8306f69eba67e8693c55e.tar.gz",
        ],
    )
