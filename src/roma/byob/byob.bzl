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

def _byob_image(
        *,
        name,
        cmd,
        user,
        debug,
        use_nonroot,
        entrypoint,
        layer_tars,
        repo_tags,
        udf_binary_labels):
    debug_str = "debug" if debug else "nondebug"
    pkg_files(
        name = "{}_sample_udf_execs".format(name),
        srcs = udf_binary_labels,
        attributes = pkg_attributes(mode = "0500"),
        prefix = "/udf",
    )
    pkg_tar(
        name = "{}_sample_udf_tar".format(name),
        srcs = [":{}_sample_udf_execs".format(name)],
        owner = "{}.{}".format(user.uid, user.gid),
    )
    oci_image(
        name = "{}".format(name),
        base = select({
            "@platforms//cpu:aarch64": "@runtime-debian-{dbg}-{nonroot}-arm64".format(dbg = debug_str, nonroot = user.flavor),
            "@platforms//cpu:x86_64": "@runtime-debian-{dbg}-{nonroot}-amd64".format(dbg = debug_str, nonroot = user.flavor),
        }),
        cmd = cmd,
        entrypoint = entrypoint,
        tars = [
            "//src/roma/byob/container:gvisor_tar_{}".format(user.flavor),
            "//src/roma/byob/container:container_config_tar_{}".format(user.flavor),
            "//src/roma/byob/container:byob_server_container_with_dir_{}.tar".format(user.flavor),
            "//src/roma/byob/container:var_run_runsc_tar_{}".format(user.flavor),
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

def byob_image(
        name,
        *,
        user,
        cmd = [],
        udf_binary_labels,
        layer_tars = [],
        repo_tags = [],
        debug = False,
        use_nonroot = True,
        entrypoint = ["/busybox/sh"]):
    """
        Generates a BYOB OCI container image:
          * {name}
        Each image has a corresponding OCI tarball:
          * {name}_tarball

        cmd: The resulting image's command. Refer to rules_oci oci_image.cmd.
        udf_binary_labels: Target labels of the udf binaries.
        entrypoint: The resulting image's entrypoint.
        layer_tars: List of tar files to add to the image as layers. Refer to rules_oci oci_image.tars.
        repo_tags: Tags for the resulting image.
    """
    _byob_image(
        name = name,
        cmd = cmd,
        user = user,
        debug = debug,
        entrypoint = entrypoint,
        layer_tars = layer_tars,
        repo_tags = repo_tags + ["{}:latest".format(name)],
        udf_binary_labels = udf_binary_labels,
        use_nonroot = use_nonroot,
    )
