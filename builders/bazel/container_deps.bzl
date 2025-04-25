# Copyright 2025 Google LLC
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

load("@rules_oci//oci:pull.bzl", "oci_pull")

_images = {
    "runtime-debian-debug-nonroot": {
        "arch_hashes": {
            # Mar 28, 2025
            "amd64": "0a8aecaca4eec99be3b8034bcbd4a7541c6dc27c5a1eb9c80b0ba2f597c1e3f1",
            # Mar 28, 2025
            "arm64": "e5e85d96a09bef71cf1b6a1b21eadfb7d64f5e1c9bd4222d6762f7464f0e72f4",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian12",
    },
    "runtime-debian-debug-root": {
        # Debug build so we can use 'sh'. Root, for GCP coordinators
        # auth to work
        "arch_hashes": {
            # Mar 28, 2025
            "amd64": "5d6d0b2f650f41ec0ed490f7b12f3e326af5964517a8a8dc8c65e9dfa4bd8008",
            # Mar 28, 2025
            "arm64": "11e03b8351fa3dc15cea2a97a3484617adeadab37045f77d5328046f0fbd9544",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian12",
    },
    "runtime-debian-nondebug-nonroot": {
        # This image contains a minimal Linux, glibc runtime for
        # "mostly-statically compiled" languages like Rust and D.
        # https://github.com/GoogleContainerTools/distroless/blob/main/cc/README.md
        "arch_hashes": {
            "amd64": "acf6c2fe4179cd5da18bcf433f0b62467c40f2a42dc821c08cc4ce2f7037813b",
            "arm64": "4e6bf5546fc17c9d434b3975daa5dbdb0e4411db4db5e96b122ba8f697f14810",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian12",
    },
    "runtime-debian-nondebug-root": {
        "arch_hashes": {
            # Mar 28, 2025
            "amd64": "dc7acdb6300eaa99ae93621b0f033237ae1284fdd5ab323b4d90ba8359c55854",
            # Mar 28, 2025
            "arm64": "b97c1911753fbbdf557e5994a5930de9233f9eec7fd0b773f6624c77b01b76db",
        },
        "registry": "gcr.io",
        "repository": "distroless/cc-debian12",
    },
    # Non-distroless; only for debugging purposes
    "runtime-ubuntu-fulldist-debug-root": {
        # Ubuntu 22.04 ubuntu:jammy-20250404
        "arch_hashes": {
            "amd64": "a76d0e9d99f0e91640e35824a6259c93156f0f07b7778ba05808c750e7fa6e68",
            "arm64": "04c0fd7fceedf5c0fe69ec1685c37cf270f03ae424322a58548b095528f4a3c3",
        },
        "registry": "docker.io",
        "repository": "library/ubuntu",
    },
}

def container_deps():
    [
        oci_pull(
            name = "{}-{}".format(img_name, arch),
            digest = "sha256:{}".format(hash),
            image = "{}/{}".format(image["registry"], image["repository"]),
        )
        for img_name, image in _images.items()
        for arch, hash in image["arch_hashes"].items()
    ]

def container_image(image, arch):
    img = _images[image]
    return struct(
        digest = "sha256:{}".format(img["arch_hashes"][arch]),
        image = "{}/{}".format(img["registry"], img["repository"]),
        uri = "{}/{}@sha256:{}".format(img["registry"], img["repository"], img["arch_hashes"][arch]),
    )

DISTROLESS_USERS = [
    struct(
        flavor = "nonroot",
        uid = 65532,
        user = "nonroot",
        gid = 65532,
        group = "nonroot",
    ),
    struct(
        flavor = "root",
        uid = 0,
        user = "root",
        gid = 0,
        group = "root",
    ),
]

def get_user(user = "nonroot"):
    """
    Extracts a struct with details from DISTROLESS_USERS based on the given user.

    Args:
      user: The user to search for (e.g., "root" or "nonroot").

    Returns:
      The struct with the matching user, or None if no match is found.
    """
    for entry in DISTROLESS_USERS:
        if entry.user == user:
            return entry
    return None
