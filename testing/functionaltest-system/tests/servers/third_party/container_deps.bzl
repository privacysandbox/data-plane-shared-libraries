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

load("@container_structure_test//:repositories.bzl", "container_structure_test_register_toolchain")
load("@rules_oci//oci:pull.bzl", "oci_pull")

def container_deps():
    images = {
        "runtime-debian": {
            "arch_hashes": {
                # stable-20230502-slim
                "amd64": "1529cbfd67815df9c001ed90a1d8fe2d91ef27fcaa5b87f549907202044465cb",
                "arm64": "87792336ff6365b90d0df3e56418fabc25034b528c5d44ce8c7aebcb0c2dfe74",
            },
            "registry": "docker.io",
            "repository": "library/debian",
        },
    }
    [
        oci_pull(
            name = img_name + "-" + arch,
            digest = "sha256:" + hash,
            image = "{reg}/{repo}".format(
                reg = image["registry"],
                repo = image["repository"],
            ),
            platforms = [
                "linux/{}".format(arch),
            ],
        )
        for img_name, image in images.items()
        for arch, hash in image["arch_hashes"].items()
    ]

    container_structure_test_register_toolchain(name = "cst")
