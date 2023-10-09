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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "acceptor_pool",
    hdrs = ["acceptor_pool.h"],
    deps = [
        ":socket_types",
        "//scp/cc/aws:include_dir",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:flat_hash_map",
    ],
)

cc_library(
    name = "buffer",
    hdrs = ["buffer.h"],
    deps = [
        ":freelist",
        "//scp/cc/aws:include_dir",
    ],
)

cc_library(
    name = "client_session_pool",
    srcs = ["client_session_pool.cc"],
    hdrs = ["client_session_pool.h"],
    deps = [
        ":socket_types",
        ":socket_vendor_protocol",
        "//scp/cc/aws:include_dir",
        "@boost//:asio",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        "//scp/cc/aws:include_dir",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "freelist",
    hdrs = ["freelist.h"],
    deps = [
        "//scp/cc/aws:include_dir",
    ],
)

cc_library(
    name = "preload",
    srcs = [
        "preload.cc",
    ],
    hdrs = ["preload.h"],
    deps = [
        ":protocol",
        ":socket_vendor_protocol",
        "//scp/cc/aws:include_dir",
    ],
)

cc_library(
    name = "protocol",
    srcs = ["protocol.cc"],
    hdrs = ["protocol.h"],
    deps = [
        "//scp/cc/aws:include_dir",
    ],
)

cc_library(
    name = "proxy_bridge",
    srcs = ["proxy_bridge.cc"],
    hdrs = ["proxy_bridge.h"],
    deps = [
        ":acceptor_pool",
        ":buffer",
        ":socket_types",
        ":socks5_state",
        "//scp/cc/aws:include_dir",
        "@boost//:asio",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "proxy_server",
    srcs = ["proxy_server.cc"],
    hdrs = ["proxy_server.h"],
    deps = [
        ":acceptor_pool",
        ":config",
        ":proxy_bridge",
        ":socket_types",
        "//scp/cc/aws:include_dir",
        "@boost//:asio",
    ],
)

cc_library(
    name = "socket_types",
    hdrs = ["socket_types.h"],
    deps = [
        "//scp/cc/aws:include_dir",
    ],
)

cc_library(
    name = "socket_vendor_protocol",
    hdrs = ["socket_vendor_protocol.h"],
    deps = [
        "//scp/cc/aws:include_dir",
    ],
)

cc_library(
    name = "socket_vendor_server",
    srcs = ["socket_vendor_server.cc"],
    hdrs = ["socket_vendor_server.h"],
    deps = [
        ":client_session_pool",
        ":socket_types",
        "//scp/cc/aws:include_dir",
        "@boost//:asio",
        "@com_github_google_glog//:glog",
    ],
)

cc_library(
    name = "socks5_state",
    srcs = ["socks5_state.cc"],
    hdrs = ["socks5_state.h"],
    deps = [
        ":buffer",
        ":protocol",
        "//scp/cc/aws:include_dir",
        "@com_github_google_glog//:glog",
    ],
)

cc_binary(
    name = "proxy",
    srcs = [":proxy.cc"],
    linkopts = ["-pthread"],
    deps = [
        ":config",
        ":proxy_server",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/debugging:failure_signal_handler",
        "@com_google_absl//absl/debugging:symbolize",
    ],
)

cc_binary(
    name = "proxy_preload",
    srcs = [
        ":preload.cc",
        ":preload.h",
        ":protocol.cc",
        ":protocol.h",
    ],
    copts = [
        "-fvisibility=hidden",
    ],
    linkopts = ["-ldl"],
    linkshared = True,
    deps = [
        ":preload",
        ":protocol",
        ":socket_vendor_protocol",
        "//scp/cc/aws:include_dir",
    ],
)

cc_binary(
    name = "proxify",
    srcs = [":proxify.cc"],
    deps = ["//scp/cc/aws:include_dir"],
)

cc_binary(
    name = "socket_vendor",
    srcs = [":socket_vendor.cc"],
    deps = [
        ":protocol",
        ":socket_vendor_server",
        "@com_github_google_glog//:glog",
    ],
)
