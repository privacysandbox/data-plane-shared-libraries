# Copyright 2024 Google LLC
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

"""Macros for defining an SUT (System Under Test)."""

load("@aspect_bazel_lib//lib:copy_to_directory.bzl", "copy_to_directory")
load("@aspect_bazel_lib//lib:glob_match.bzl", "glob_match")
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")

def sut(
        name,
        sut_dir = "**",
        sut_labels = [],
        custom_file_mappings = {},
        **kwargs):
    """Macro to define SUT targets"""

    sut_files = native.glob(
        include = ["{}/**/*".format(sut_dir)],
        exclude = ["{}/**/BUILD.bazel".format(sut_dir)],
    )

    # rename BUILD.sut files to BUILD.bazel
    sut_build_mappings = {f: f.replace("BUILD.sut", "BUILD.bazel") for f in sut_files if glob_match("**/BUILD.sut", f)}

    dir_target = "{}_sutdir".format(name)
    copy_to_directory(
        name = dir_target,
        srcs = sut_files + sut_labels,
        replace_prefixes = dict(sut_build_mappings.items() + custom_file_mappings.items()),
    )
    pkg_zip(
        name = name,
        srcs = [":{}".format(dir_target)],
        **kwargs
    )
