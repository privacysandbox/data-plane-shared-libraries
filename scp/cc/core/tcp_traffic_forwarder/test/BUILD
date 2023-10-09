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

load("@rules_cc//cc:defs.bzl", "cc_test")

cc_test(
    name = "tcp_traffic_forwarder_test",
    size = "small",
    srcs = [
        "tcp_traffic_forwarder_socat_test.cc",
    ],
    tags = ["manual"],
    deps = [
        "//scp/cc:cc_base_include_dir",
        "//scp/cc/core/interface:interface_lib",
        "//scp/cc/core/tcp_traffic_forwarder/src:core_tcp_traffic_forwarder",
        "//scp/cc/core/test:core_test_lib",
        "//scp/cc/public/core/test/interface:execution_result_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)
