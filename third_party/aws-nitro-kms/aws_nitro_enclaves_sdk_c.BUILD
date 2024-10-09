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

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

copy_file(
    name = "nsm_h",
    src = "@google_privacysandbox_servers_common//third_party/aws-nsm:nsm.h",
    out = "include/nsm.h",
)

copy_file(
    name = "json_h",
    src = "@json_c//:json_h",
    out = "include/json-c/json.h",
)

cc_library(
    name = "json",
    hdrs = [":json_h"],
    deps = [
        "@json_c",
    ],
)

cc_library(
    name = "nsm",
    hdrs = [":nsm_h"],
    deps = [
        "@google_privacysandbox_servers_common//src/cpio/client_providers/kms_client_provider/aws:libnsm_so",
    ],
)

cc_library(
    name = "aws_nitro_enclaves_sdk_c",
    srcs = [
        "source/attestation.c",
        "source/cms.c",
        "source/kms.c",
        "source/nitro_enclaves.c",
        "source/rest.c",
    ],
    hdrs = [
        "include/aws/nitro_enclaves/attestation.h",
        "include/aws/nitro_enclaves/exports.h",
        "include/aws/nitro_enclaves/internal/cms.h",
        "include/aws/nitro_enclaves/kms.h",
        "include/aws/nitro_enclaves/nitro_enclaves.h",
        "include/aws/nitro_enclaves/rest.h",
    ],
    includes = [
        "include",
    ],
    linkopts = [
        "-Wl,-rpath,'$$ORIGIN'",
    ],
    deps = [
        ":json",
        ":nsm",
        "@nitrokmscli_aws_c_auth//:aws_c_auth",
        "@nitrokmscli_aws_c_common//:aws_c_common",
        "@nitrokmscli_aws_c_http//:aws_c_http",
        "@nitrokmscli_aws_c_io//:aws_c_io",
    ],
)

cc_binary(
    name = "aws_nitro_enclaves_cli",
    srcs = [
        "bin/kmstool-enclave-cli/main.c",
    ],
    linkopts = [
        "-fuse-ld=lld",
        "-Wl,-rpath,'$$ORIGIN/../lib'",
    ],
    visibility = ["//visibility:public"],
    deps = [
        ":aws_nitro_enclaves_sdk_c",
    ],
)
