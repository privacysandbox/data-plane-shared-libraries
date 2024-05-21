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

load("@io_bazel_rules_docker//container:container.bzl", "container_pull")
load("@rules_oci//oci:pull.bzl", "oci_pull")

def container_deps():
    images = {
        "runtime-debian-debug-nonroot": {
            "arch_hashes": {
                # cc-debian11:debug-nonroot
                "amd64": "72b9108b17a4ef0398998d45cbc14af2f3270af374fc2aa2c74823c6c7054fac",
                "arm64": "623676598d55f93ff93ea3b9d95f3cb5a379eca66dfcf9b2734f2cc3e5f34666",
            },
            "registry": "gcr.io",
            "repository": "distroless/cc-debian11",
        },
        "runtime-debian-debug-root": {
            # debug build so we can use 'sh'. Root, for gcp coordinators
            # auth to work
            "arch_hashes": {
                "amd64": "d5a2169bc2282598f0cf886a3d301269d0ee5bf7f7392184198dd41d36b70548",
                "arm64": "6449313a9a80b2758f505c81462c492da87f76954d319f2adb55401177798cce",
            },
            "registry": "gcr.io",
            "repository": "distroless/cc-debian11",
        },
        # Non-distroless; only for debugging purposes
        "runtime-ubuntu-fulldist-debug-root": {
            # Ubuntu 20.04
            "arch_hashes": {
                "amd64": "81bba8d1dde7fc1883b6e95cd46d6c9f4874374f2b360c8db82620b33f6b5ca1",
                "arm64": "ca165754e2f953a4f686409b1eb5855212f42a252462c9c50bbc3077f3b9a654",
            },
            "registry": "docker.io",
            "repository": "library/ubuntu",
        },
    }

    [
        oci_pull(
            name = "{}-{}".format(img_name, arch),
            digest = "sha256:" + hash,
            image = "{}/{}".format(image["registry"], image["repository"]),
        )
        for img_name, image in images.items()
        for arch, hash in image["arch_hashes"].items()
    ]

    [
        container_pull(
            name = "{}-{}-docker".format(img_name, arch),
            digest = "sha256:" + hash,
            registry = image["registry"],
            repository = image["repository"],
        )
        for img_name, image in images.items()
        for arch, hash in image["arch_hashes"].items()
    ]
