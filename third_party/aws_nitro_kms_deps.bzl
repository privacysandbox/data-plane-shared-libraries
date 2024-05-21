# Copyright 2023 Google LLC
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

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

def aws_nitro_kms_repos():
    """
    Import AWS Nitro KMS repositories
    """
    maybe(
        http_archive,
        name = "nitrokmscli_aws_lc",
        build_file = Label("//third_party/aws-nitro-kms:aws_lc.BUILD"),
        patch_args = ["-p1"],
        patches = [Label("//third_party/aws-nitro-kms:aws_lc.patch")],
        sha256 = "22c56082a003342c3690d8f51edadf98e0614804f5cc42be1600bb005e9c5d98",
        strip_prefix = "aws-lc-1.12.0",
        urls = [
            "https://github.com/aws/aws-lc/archive/refs/tags/v1.12.0.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_io",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_io.BUILD"),
        patch_args = ["-p1"],
        patches = [Label("//third_party/aws-nitro-kms:aws_c_io.patch")],
        sha256 = "67634800b02f0451b573a51e1bb01ee34d50a034e8586d0d3f1cc60bb533a8ec",
        strip_prefix = "aws-c-io-0.11.0",
        urls = [
            "https://github.com/awslabs/aws-c-io/archive/refs/tags/v0.11.0.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_s2n_tls",
        build_file = Label("//third_party/aws-nitro-kms:s2n_tls.BUILD"),
        sha256 = "34c3d3f72011f9e7b96d4ea129dc66719046688c0fcf5dfded6b493d26faf534",
        strip_prefix = "s2n-tls-1.3.46",
        urls = [
            "https://github.com/aws/s2n-tls/archive/refs/tags/v1.3.46.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_common",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_common.BUILD"),
        sha256 = "bbcce705079c32f36030d9275f0ea5b8d69c22be1bd06db1e757067798c101b0",
        strip_prefix = "aws-c-common-0.8.0",
        urls = [
            "https://github.com/awslabs/aws-c-common/archive/refs/tags/v0.8.0.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_sdkutils",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_sdkutils.BUILD"),
        sha256 = "72b03e8e3d15479fe1a2807a3dc65ad87ea3c33495ea3aafb57fc83927b4061c",
        strip_prefix = "aws-c-sdkutils-0.1.2",
        urls = [
            "https://github.com/awslabs/aws-c-sdkutils/archive/refs/tags/v0.1.2.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_cal",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_cal.BUILD"),
        sha256 = "a255500ee0980016e7e5a24701b7d9017b0d1a5f41c3e261581e5b47ae1ab77c",
        strip_prefix = "aws-c-cal-0.5.18",
        urls = [
            "https://github.com/awslabs/aws-c-cal/archive/refs/tags/v0.5.18.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_compression",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_compression.BUILD"),
        sha256 = "8495e7758407ea478a35c6b86989c4ff3cb248245c2f9b8fb83dd388e2ac8d3e",
        strip_prefix = "aws-c-compression-0.2.14",
        urls = [
            "https://github.com/awslabs/aws-c-compression/archive/refs/tags/v0.2.14.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_http",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_http.BUILD"),
        sha256 = "d3baff4455bf34faaee3859088e73c1ccb2ba1b92af21935ec66f74eca355c28",
        strip_prefix = "aws-c-http-0.7.6",
        urls = [
            "https://github.com/awslabs/aws-c-http/archive/refs/tags/v0.7.6.zip",
        ],
    )

    maybe(
        http_archive,
        name = "nitrokmscli_aws_c_auth",
        build_file = Label("//third_party/aws-nitro-kms:aws_c_auth.BUILD"),
        sha256 = "7c06ef2d2d1dee1bc61cf23c03deb1c88e8825ee51b071fd6d6c0ced150348d8",
        strip_prefix = "aws-c-auth-0.6.22",
        urls = [
            "https://github.com/awslabs/aws-c-auth/archive/refs/tags/v0.6.22.zip",
        ],
    )

    maybe(
        http_archive,
        name = "json_c",
        build_file = Label("//third_party/json-c:json_c.BUILD"),
        sha256 = "471e9eb1dad4fd2e4fec571d8415993e66a89f23a5b052f1ba11b54db90252de",
        strip_prefix = "json-c-json-c-0.17-20230812",
        urls = [
            "https://github.com/json-c/json-c/archive/refs/tags/json-c-0.17-20230812.zip",
        ],
    )

    maybe(
        http_archive,
        name = "aws-nitro-enclaves-nsm-api",
        build_file = Label("//third_party/aws-nsm:aws_nitro_enclaves_nsm_api.BUILD"),
        sha256 = "8150bb1e9e757f24ff35b19c10b924e2d96ed2a81f98efe05048c50e2e0804e6",
        strip_prefix = "aws-nitro-enclaves-nsm-api-0.4.0",
        urls = ["https://github.com/aws/aws-nitro-enclaves-nsm-api/archive/refs/tags/v0.4.0.zip"],
    )
