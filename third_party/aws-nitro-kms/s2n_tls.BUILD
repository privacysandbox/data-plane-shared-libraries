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

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "s2n_tls",
    srcs = glob([
        "crypto/*.c",
        "crypto/*.h",
        "error/*.c",
        "error/*.h",
        "pq-crypto/*.c",
        "pq-crypto/*.h",
        "stuffer/*.c",
        "stuffer/*.h",
        "tls/*.c",
        "tls/*.h",
        "tls/*/*.c",
        "tls/*/*.h",
        "utils/*.c",
        "utils/*.h",
    ]),
    hdrs = glob([
        "api/unstable/*.h",
    ]) + ["api/s2n.h"],
    defines = [
        "BUILD_S2N=true",
        "BUILD_SHARED_LIBS=ON",
        "BUILD_TESTING=0",
        "DISABLE_WERROR=ON",
        "S2N_LIBCRYPTO=awslc",
        "S2N_NO_PQ",
    ],
    includes = [
        "api",
    ],
    deps = [
        "@nitrokmscli_aws_lc//:crypto",
        "@nitrokmscli_aws_lc//:ssl",
    ],
)
