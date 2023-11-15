# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the  "ssl/License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an  "ssl/AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load(
    "@google_privacysandbox_servers_common//third_party/aws-nitro-kms:aws_lc.bzl",
    "crypto_hdrs",
    "crypto_internal_hdrs",
    "crypto_srcs",
    "fips_fragments",
    "ssl_hdrs",
    "ssl_internal_hdrs",
    "ssl_srcs",
)
load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "ssl",
    srcs = ssl_srcs + ssl_internal_hdrs,
    hdrs = ssl_hdrs,
    includes = ["include"],
    deps = [
        ":crypto",
    ],
)

cc_library(
    name = "crypto",
    srcs = crypto_srcs + crypto_internal_hdrs,
    hdrs = crypto_hdrs + fips_fragments,
    includes = ["include"],
    linkopts = ["-pthread"],
)
