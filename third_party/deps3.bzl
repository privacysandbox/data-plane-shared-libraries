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

load("@aspect_rules_js//npm:npm_import.bzl", "npm_translate_lock", "pnpm_repository")
load("@com_github_google_rpmpack//:deps.bzl", "rpmpack_dependencies")
load("@com_github_googleapis_google_cloud_cpp//bazel:google_cloud_cpp_deps.bzl", "google_cloud_cpp_deps")
load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")
load("@com_google_sandboxed_api//sandboxed_api/bazel:llvm_config.bzl", "llvm_disable_optional_support_deps")
load("@com_google_sandboxed_api//sandboxed_api/bazel:sapi_deps.bzl", "sapi_deps")
load("@depend_on_what_you_use//:setup_step_2.bzl", dwyu_setup_step_2 = "setup_step_2")
load("@google_benchmark//:bazel/benchmark_deps.bzl", "benchmark_deps")
load("@io_opentelemetry_cpp//bazel:repository.bzl", "opentelemetry_cpp_deps")
load("@rules_buf//buf:repositories.bzl", "rules_buf_dependencies", "rules_buf_toolchains")
load("@rules_fuzzing//fuzzing:init.bzl", "rules_fuzzing_init")
load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")
load("@rules_proto_grpc//:repositories.bzl", "rules_proto_grpc_repos", "rules_proto_grpc_toolchains")
load("@rules_proto_grpc//go:repositories.bzl", rules_proto_grpc_go_repos = "go_repos")
load("@rules_rust//crate_universe:defs.bzl", "crates_repository")
load("@tink_cc//:tink_cc_deps.bzl", "tink_cc_deps")
load("@v8_python_deps//:requirements.bzl", install_v8_python_deps = "install_deps")
load("//third_party:aws_nitro_kms_deps.bzl", "aws_nitro_kms_repos")
load("//third_party:bazel_rules_closure.bzl", "bazel_rules_closure")

def _npm_deps():
    pnpm_repository(name = "pnpm")

    npm_translate_lock(
        name = "npm",
        npmrc = Label("//:.npmrc"),
        pnpm_lock = Label("//:pnpm-lock.yaml"),
        verify_node_modules_ignored = Label("//:.bazelignore"),
    )

def deps3():
    protobuf_deps()
    rules_proto_grpc_toolchains()
    rules_proto_grpc_repos()
    rules_proto_grpc_go_repos()
    rules_proto_dependencies()
    rules_proto_toolchains()
    google_cloud_cpp_deps()
    llvm_disable_optional_support_deps()
    sapi_deps()
    bazel_rules_closure()
    rpmpack_dependencies()
    install_v8_python_deps()
    rules_buf_dependencies()
    rules_buf_toolchains(
        version = "v1.39.0",
        sha256 = "a13fa6afecb3ba0cbf1b1fd4ca2a0f1d0cd935619d20a6b9a069aab9fb31ed05",
    )
    tink_cc_deps()
    switched_rules_by_language(
        name = "com_google_googleapis_imports",
        cc = True,
        grpc = True,
    )
    opentelemetry_cpp_deps()
    rules_fuzzing_init()

    # repin aws-nsm deps using:
    #   EXTRA_DOCKER_RUN_ARGS="--env=CARGO_BAZEL_REPIN=1" builders/tools/bazel-debian sync --only=aws_nsm_crate_index
    crates_repository(
        name = "aws_nsm_crate_index",
        cargo_lockfile = Label("//third_party/aws-nsm:Cargo.lock"),
        lockfile = Label("//third_party/aws-nsm:Cargo.Bazel.lock"),
        manifests = [
            "@aws-nitro-enclaves-nsm-api//:Cargo.toml",
            "@aws-nitro-enclaves-nsm-api//:nsm-lib/Cargo.toml",
            "@aws-nitro-enclaves-nsm-api//:nsm-test/Cargo.toml",
        ],
    )

    # repin deps using:
    #   EXTRA_DOCKER_RUN_ARGS="--env=CARGO_BAZEL_REPIN=1" builders/tools/bazel-debian sync --only=cddl_crate_index
    crates_repository(
        name = "cddl_crate_index",
        quiet = False,
        cargo_lockfile = Label("cddl/Cargo.lock"),
        lockfile = Label("cddl/cargo-bazel-lock.json"),
        manifests = [
            Label("cddl/Cargo.toml"),
        ],
    )
    aws_nitro_kms_repos()
    benchmark_deps()
    dwyu_setup_step_2()
    _npm_deps()
