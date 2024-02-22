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

load("@bazel_tools//tools/build_defs/repo:git.bzl", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")
load("@rules_python//python:pip.bzl", "pip_parse")

def import_v8():
    maybe(
        http_archive,
        name = "v8",
        patch_args = ["-p1"],
        patches = [Label("//build_defs/cc:v8.patch")],
        sha256 = "33f996c0b47cad6b492130d4f7ae8180d55a4d1bd4ff639b5c43ca56ffb7f5c1",
        strip_prefix = "v8-11.1.277.9",
        urls = ["https://github.com/v8/v8/archive/refs/tags/11.1.277.9.zip"],
    )

    pip_parse(
        name = "v8_python_deps",
        extra_pip_args = ["--require-hashes"],
        requirements_lock = "@v8//:bazel/requirements.txt",
    )

    new_git_repository(
        name = "com_googlesource_chromium_icu",
        build_file = "@v8//:bazel/BUILD.icu",
        commit = "7ff1e9befce5567754dc88392dfaa4704e261ab3",
        remote = "https://chromium.googlesource.com/chromium/deps/icu.git",
        shallow_since = "1676660986 +0000",
        patch_cmds = ["find source -name BUILD.bazel | xargs rm"],
    )

    native.bind(
        name = "icu",
        actual = "@com_googlesource_chromium_icu//:icu",
    )

    native.bind(
        name = "zlib_compression_utils",
        actual = "@com_googlesource_chromium_zlib//:zlib_compression_utils",
    )

    new_git_repository(
        name = "com_googlesource_chromium_base_trace_event_common",
        build_file = "@v8//:bazel/BUILD.trace_event_common",
        commit = "521ac34ebd795939c7e16b37d9d3ddb40e8ed556",
        remote = "https://chromium.googlesource.com/chromium/src/base/trace_event/common.git",
        shallow_since = "1662669790 -0700",
    )

    native.bind(
        name = "base_trace_event_common",
        actual = "@com_googlesource_chromium_base_trace_event_common//:trace_event_common",
    )
