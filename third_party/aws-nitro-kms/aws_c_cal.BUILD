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

# Description:
#   AWS C Crypto Abstraction Layer

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "aws_c_cal",
    srcs = glob([
        "include/aws/cal/*.h",
        "include/aws/cal/private/*.h",
        "source/*.c",
        "source/unix/*.c",
    ]),
    includes = [
        "include",
    ],
    deps = [
        "@nitrokmscli_aws_c_common//:aws_c_common",
        "@nitrokmscli_aws_lc//:crypto",
        "@nitrokmscli_aws_lc//:ssl",
    ],
)
