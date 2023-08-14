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

def container_deps():
    images = {
        ## Distroless image for running Java
        #"runtime-java": {
        #    "arch_hashes": {
        #        # debug-nonroot 2023-02-15
        #        "amd64": "dca8c4ccea3797aa8df1cec14b7401b8b3868392ac2cd06ab2ca311d52ae7b98",
        #        "arm64": "5e2475b1be5b81e9dc2527d71d4eff06cb192dff61c926e6ab10333f63ad308e",
        #    },
        #    "registry": "gcr.io",
        #    "repository": "distroless/java11-debian11",
        #},
    }

    [
        container_pull(
            name = img_name + "-" + arch,
            digest = "sha256:" + hash,
            registry = image["registry"],
            repository = image["repository"],
        )
        for img_name, image in images.items()
        for arch, hash in image["arch_hashes"].items()
    ]
