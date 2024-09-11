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

load("@container_structure_test//:defs.bzl", "container_structure_test")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("//third_party:container_deps.bzl", "DISTROLESS_USERS")

def _byob_image(
        *,
        name,
        user,
        debug,
        repo_tags,
        udf_binary_labels,
        **kwargs):
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
        labels = {"tee.launch_policy.log_redirect": "always"},
        tars = [
            Label("//src/roma/byob/container:gvisor_tar_{}".format(user.flavor)),
            Label("//src/roma/byob/container:container_config_tar_{}".format(user.flavor)),
            Label("//src/roma/byob/container:byob_server_container_with_dir_{}.tar".format(user.flavor)),
            Label("//src/roma/byob/container:var_run_runsc_tar_{}".format(user.flavor)),
        ] + kwargs.get("tars", []) + [
            ":{}_sample_udf_tar".format(name),
        ],
        **{k: v for (k, v) in kwargs.items() if k not in ["base", "tars"]}
    )
    container_structure_test(
        name = "{}_byob_test".format(name),
        size = "small",
        configs = [Label("//src/roma/byob:image_{}_test.yaml".format(user.flavor))],
        image = ":{}".format(name),
        tags = ["noasan"],
    )
    if repo_tags:
        oci_load(
            name = "{}_tarball".format(name),
            image = ":{}".format(name),
            repo_tags = repo_tags,
        )
        native.filegroup(
            name = "{}_tarball.tar".format(name),
            srcs = [":{}_tarball".format(name)],
            output_group = "tarball",
        )

def byob_image(
        name,
        *,
        udf_binary_labels,
        repo_tags = [],
        debug = False,
        use_nonroot = True,
        **kwargs):
    """
        Generates a BYOB OCI container image:
          * {name}
        Each image has a corresponding OCI tarball:
          * {name}_tarball.tar

        udf_binary_labels: Target labels of the udf binaries.
        repo_tags: Tags for the resulting image.
        **kwargs: keyword args passed through to oci_image.
        Note: base arg cannot be overridden and will be ignored.
    """
    flavor = "nonroot" if use_nonroot else "root"
    user = [u for u in DISTROLESS_USERS if u.flavor == flavor][0]
    default_entrypoint = ["/busybox/sh"] if debug else []
    _byob_image(
        name = name,
        user = user,
        debug = debug,
        entrypoint = kwargs.get("entrypoint", default_entrypoint),
        repo_tags = repo_tags + ["{}:latest".format(name)],
        udf_binary_labels = udf_binary_labels,
        **{k: v for (k, v) in kwargs.items() if k not in ["entrypoint"]}
    )
