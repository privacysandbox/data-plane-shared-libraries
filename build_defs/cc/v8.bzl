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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@rules_python//python:pip.bzl", "pip_parse")

def import_v8():
    maybe(
        http_archive,
        name = "v8",
        patch_args = ["-p1"],
        patches = [Label("//build_defs/cc:v8.patch")],
        sha256 = "f2da3da236c1240befb40d1d5e5658e09a0b16219de087a7d1bda098795de017",
        strip_prefix = "v8-11.9.172",
        urls = ["https://github.com/v8/v8/archive/refs/tags/11.9.172.zip"],
    )

    pip_parse(
        name = "v8_python_deps",
        extra_pip_args = ["--require-hashes"],
        requirements_lock = "@v8//:bazel/requirements.txt",
    )

    http_archive(
        name = "com_googlesource_chromium_icu",
        build_file = "@v8//:bazel/BUILD.icu",
        patch_cmds = ["find source -name BUILD.bazel | xargs rm"],
        # ICU 74 for Chromium: https://chromium.googlesource.com/chromium/deps/icu/+/refs/heads/chromium/74staging
        # sha256 is unstable for this url
        urls = ["https://chromium.googlesource.com/chromium/deps/icu/+archive/ef208e4799590d594bf482f05d6575a73423e184.tar.gz"],
    )

    native.bind(
        name = "icu",
        actual = "@com_googlesource_chromium_icu//:icu",
    )

    native.bind(
        name = "zlib_compression_utils",
        actual = "@com_googlesource_chromium_zlib//:zlib_compression_utils",
    )

    native.bind(
        name = "absl_optional",
        actual = "@com_google_absl//absl/types:optional",
    )

    native.bind(
        name = "absl_btree",
        actual = "@com_google_absl//absl/container:btree",
    )

    native.bind(
        name = "absl_flat_hash_map",
        actual = "@com_google_absl//absl/container:flat_hash_map",
    )

    native.bind(
        name = "absl_flat_hash_set",
        actual = "@com_google_absl//absl/container:flat_hash_set",
    )

    http_archive(
        name = "com_googlesource_chromium_base_trace_event_common",
        build_file = "@v8//:bazel/BUILD.trace_event_common",
        urls = ["https://chromium.googlesource.com/chromium/src/base/trace_event/common.git/+archive/29ac73db520575590c3aceb0a6f1f58dda8934f6.tar.gz"],
    )

    native.bind(
        name = "base_trace_event_common",
        actual = "@com_googlesource_chromium_base_trace_event_common//:trace_event_common",
    )
