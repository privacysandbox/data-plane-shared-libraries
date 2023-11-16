# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Further initialization of shared control plane dependencies."""

load("@aws_nsm_crate_index//:defs.bzl", "crate_repositories")
load(
    "@io_bazel_rules_closure//closure:repositories.bzl",
    "rules_closure_dependencies",
    "rules_closure_toolchains",
)
load("@io_bazel_rules_docker//repositories:deps.bzl", container_deps = "deps")
load("@io_bazel_rules_docker//repositories:go_repositories.bzl", "go_deps")
load("@rules_buf//gazelle/buf:repositories.bzl", "gazelle_buf_dependencies")

def deps4():
    container_deps()
    go_deps()
    gazelle_buf_dependencies()
    rules_closure_dependencies()
    rules_closure_toolchains()

    crate_repositories()
