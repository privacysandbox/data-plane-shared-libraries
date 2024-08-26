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

load(
    "@bazel_tools//tools/build_defs/repo:http.bzl",
    "http_archive",
    "http_file",
)
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

GVISOR_VERSION = "20240807.0"

def cc_utils():
    maybe(
        http_archive,
        name = "nlohmann_json",
        build_file = Label("//build_defs/cc/shared/build_targets:nlohmann.BUILD"),
        sha256 = "e5c7a9f49a16814be27e4ed0ee900ecd0092bfb7dbfca65b5a421b774dccaaed",
        urls = [
            "https://github.com/nlohmann/json/releases/download/v3.11.2/include.zip",
        ],
    )

    maybe(
        http_archive,
        name = "oneTBB",
        # Release 2021.10.0 dated 2023-07-24
        sha256 = "78fb7bb29b415f53de21a68c4fdf97de8ae035090d9ee9caa221e32c6e79567c",
        strip_prefix = "oneTBB-2021.10.0",
        urls = ["https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.10.0.zip"],
    )

    maybe(
        http_archive,
        name = "rules_fuzzing",
        sha256 = "e6bc219bfac9e1f83b327dd090f728a9f973ee99b9b5d8e5a184a2732ef08623",
        strip_prefix = "rules_fuzzing-0.5.2",
        urls = ["https://github.com/bazelbuild/rules_fuzzing/archive/v0.5.2.zip"],
    )

    maybe(
        http_archive,
        name = "com_google_tcmalloc",
        sha256 = "d4bef5f15eb54cf481d6da96ffa6616a090009e94b336385445ee09427d8e512",
        strip_prefix = "tcmalloc-cda5074afb75ab02cf4d621986308ab7421dbbf8",
        urls = ["https://github.com/google/tcmalloc/archive/cda5074afb75ab02cf4d621986308ab7421dbbf8.zip"],
    )

    _gvisor_utils()

def _gvisor_utils(version = GVISOR_VERSION):
    release_url = "https://storage.googleapis.com/gvisor/releases/release/{}".format(version)
    maybe(
        http_file,
        name = "gvisor_runsc_amd64",
        downloaded_file_path = "runsc",
        executable = True,
        sha256 = "6fa2a311b93c4f772efa38f041eab476bc51b72f0af99968ea733e55551aead3",
        urls = ["{}/x86_64/runsc".format(release_url)],
    )

    maybe(
        http_file,
        name = "gvisor_containerd_amd64",
        downloaded_file_path = "containerd-shim-runsc-v1",
        executable = True,
        sha256 = "106e7d56332c6e374603b44426fe0ff019c7e6e99eaaa12dccec9cf7edd3e705",
        urls = ["{}/x86_64/containerd-shim-runsc-v1".format(release_url)],
    )

    maybe(
        http_file,
        name = "gvisor_runsc_arm64",
        downloaded_file_path = "runsc",
        executable = True,
        sha256 = "5ebba92b7670c67be08fe849b65a6c91dd7802158637a1baf3e5437d13b5efd5",
        urls = ["{}/aarch64/runsc".format(release_url)],
    )

    maybe(
        http_file,
        name = "gvisor_containerd_arm64",
        downloaded_file_path = "containerd-shim-runsc-v1",
        executable = True,
        sha256 = "45d32d3c7ff9bf455a94c3ba3021ad4ae9c3619ce680faf880dc50b49eb2ed48",
        urls = ["{}/aarch64/containerd-shim-runsc-v1".format(release_url)],
    )
