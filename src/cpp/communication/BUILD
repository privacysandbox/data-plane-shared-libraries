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

load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")

package(default_visibility = [
    "//visibility:public",
])

cc_library(
    name = "json_utils",
    hdrs = [
        "json_utils.h",
    ],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/status:statusor",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "json_utils_test",
    size = "small",
    srcs = [
        "json_utils_test.cc",
    ],
    deps = [
        ":json_utils",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ohttp_utils",
    srcs = ["ohttp_utils.cc"],
    hdrs = ["ohttp_utils.h"],
    deps = [
        "//src/cpp/encryption/key_fetcher/interface:private_key_fetcher_interface",
        "//src/cpp/encryption/key_fetcher/interface:public_key_fetcher_interface",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_google_absl//absl/hash",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "ohttp_utils_test",
    size = "small",
    srcs = [
        "ohttp_utils_test.cc",
    ],
    deps = [
        ":ohttp_utils",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "encoding_utils",
    srcs = ["encoding_utils.cc"],
    hdrs = ["encoding_utils.h"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:oblivious_http_unstable_api",
        "@com_google_absl//absl/hash",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "encoding_utils_test",
    size = "small",
    srcs = [
        "encoding_utils_test.cc",
    ],
    deps = [
        ":encoding_utils",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "compression",
    srcs = [
        "compression.cc",
        "compression_brotli.cc",
        "uncompressed.cc",
    ],
    hdrs = [
        "compression.h",
        "compression_brotli.h",
        "uncompressed.h",
    ],
    deps = [
        "@brotli//:brotlidec",
        "@brotli//:brotlienc",
        "@com_github_google_glog//:glog",
        "@com_github_google_quiche//quiche:quiche_unstable_api",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "uncompressed_test",
    srcs = ["uncompressed_test.cc"],
    deps = [
        ":compression",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "compression_brotli_test",
    srcs = ["compression_brotli_test.cc"],
    deps = [
        ":compression",
        "@com_github_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)
