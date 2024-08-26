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

load("@rules_oci//oci:defs.bzl", "oci_image", "oci_tarball")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")

def byob_image(
        name,
        *,
        cmd = [],
        udf_binary_label,
        layer_tars = [],
        repo_tags = [],
        entrypoint = ["/bin/bash"]):
    pkg_files(
        name = "{}_sample_udf_execs".format(name),
        srcs = [
            udf_binary_label,
        ],
        attributes = pkg_attributes(mode = "0555"),
        prefix = "/udf",
    )

    pkg_tar(
        name = "{}_sample_udf_tar".format(name),
        srcs = [
            ":{}_sample_udf_execs".format(name),
        ],
    )

    oci_image(
        name = name,
        base = select({
            "@platforms//cpu:aarch64": "@runtime-ubuntu-fulldist-debug-root-arm64",
            "@platforms//cpu:x86_64": "@runtime-ubuntu-fulldist-debug-root-amd64",
        }),
        cmd = cmd,
        entrypoint = entrypoint,
        tars = [
            "//src/roma/byob/container:gvisor_tar",
            "//src/roma/byob/container:byob_server_container_with_dir.tar",
        ] + layer_tars + [
            ":{}_sample_udf_tar".format(name),
        ],
    )

    if repo_tags:
        oci_tarball(
            name = "{}_tarball".format(name),
            image = ":{}".format(name),
            repo_tags = repo_tags,
        )
