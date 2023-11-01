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

load("@com_github_google_rpmpack//:def.bzl", "pkg_tar2rpm")
load("@rules_pkg//:pkg.bzl", "pkg_tar")

package(
    default_visibility = ["//visibility:public"],
)

pkg_tar(
    name = "proxy_bin",
    srcs = [
        # Use files built inside AmazonLinux2 container to maintain compatability with AL2.
        ":libproxy_preload.so",
        ":proxify",
        ":proxy",
        ":socket_vendor",
    ],
    mode = "0755",
    ownername = "root.root",
    package_dir = "opt/google/proxy",
    tags = ["manual"],
)

pkg_tar(
    name = "proxy_service",
    srcs = [
        "vsockproxy.service",
    ],
    mode = "0644",
    ownername = "root.root",
    package_dir = "etc/systemd/system",
    tags = ["manual"],
)

pkg_tar(
    name = "proxy_tar",
    ownername = "root.root",
    tags = ["manual"],
    deps = [
        ":proxy_bin",
        ":proxy_service",
    ],
)

pkg_tar2rpm(
    name = "vsockproxy_rpm",
    data = ":proxy_tar",
    pkg_name = "vsockproxy",
    # Signal to systemd that the service files changed on install.
    postin = """
    systemctl daemon-reload
    """,
    # Signal to systemd that the service files changed on uninstall.
    postun = """
    systemctl daemon-reload
    """,
    # Stop service before uninstall starts.
    preun = """
    systemctl stop vsockproxy.service
    systemctl disable vsockproxy.service
    """,
    release = "0",
    tags = ["manual"],
    use_dir_allowlist = True,
    version = "0.0.1",
)
