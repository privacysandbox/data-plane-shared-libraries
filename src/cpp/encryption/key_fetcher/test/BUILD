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

load("@rules_cc//cc:defs.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_test(
    name = "public_key_fetcher_test",
    size = "small",
    srcs = ["public_key_fetcher_test.cc"],
    deps = [
        "//src/cpp/encryption/key_fetcher/src:public_key_fetcher",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "private_key_fetcher_test",
    size = "small",
    srcs = ["private_key_fetcher_test.cc"],
    deps = [
        "//src/cpp/encryption/key_fetcher/src:private_key_fetcher",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "key_fetcher_manager_test",
    size = "small",
    srcs = ["key_fetcher_manager_test.cc"],
    deps = [
        "//src/cpp/encryption/key_fetcher/mock:mock_private_key_fetcher",
        "//src/cpp/encryption/key_fetcher/mock:mock_public_key_fetcher",
        "//src/cpp/encryption/key_fetcher/src:key_fetcher_manager",
        "@com_github_grpc_grpc//:grpc++",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/synchronization",
        "@com_google_absl//absl/time",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "key_fetcher_utils_test",
    size = "small",
    srcs = ["key_fetcher_utils_test.cc"],
    deps = [
        "//src/cpp/encryption/key_fetcher/src:key_fetcher_utils",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)
