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
#   JSON implementation in C

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

copy_file(
    name = "config_h",
    src = "@google_privacysandbox_servers_common//third_party/json-c:json_c.config.h",
    out = "config.h",
)

copy_file(
    name = "json_config_h",
    src = "@google_privacysandbox_servers_common//third_party/json-c:json_c.json_config.h",
    out = "json_config.h",
)

copy_file(
    name = "json_h",
    src = "@google_privacysandbox_servers_common//third_party/json-c:json_c.json.h",
    out = "json.h",
    visibility = [
        "@nitrokmscli_aws_nitro_enclaves_sdk_c//:__pkg__",
    ],
)

cc_library(
    name = "json_c",
    srcs = [
        "arraylist.c",
        "arraylist.h",
        "debug.c",
        "debug.h",
        "json_c_version.c",
        "json_c_version.h",
        "json_inttypes.h",
        "json_object.c",
        "json_object.h",
        "json_object_iterator.c",
        "json_object_iterator.h",
        "json_object_private.h",
        "json_patch.c",
        "json_patch.h",
        "json_pointer.c",
        "json_pointer.h",
        "json_pointer_private.h",
        "json_tokener.c",
        "json_tokener.h",
        "json_types.h",
        "json_util.c",
        "json_util.h",
        "json_visit.c",
        "json_visit.h",
        "libjson.c",
        "linkhash.c",
        "linkhash.h",
        "math_compat.h",
        "printbuf.c",
        "printbuf.h",
        "random_seed.c",
        "random_seed.h",
        "snprintf_compat.h",
        "strdup_compat.h",
        "strerror_override.c",
        "strerror_override.h",
        "vasprintf_compat.h",
        ":config.h",
        ":json.h",
        ":json_config.h",
    ],
    visibility = [
        "@nitrokmscli_aws_nitro_enclaves_sdk_c//:__pkg__",
    ],
)

cc_binary(
    name = "json-c",
    linkopts = [
        "-static",
        "-ldl",
    ],
    linkshared = True,
    visibility = ["//visibility:public"],
    deps = [":json_c"],
)
