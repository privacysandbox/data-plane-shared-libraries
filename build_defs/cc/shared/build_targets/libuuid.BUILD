# Copyright 2024 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

cc_library(
    name = "common",
    srcs = [
        "lib/md5.c",
        "lib/randutils.c",
        "lib/sha1.c",
        "lib/strutils.c",
    ],
    hdrs = [
        "include/all-io.h",
        "include/bitops.h",
        "include/c.h",
        "include/md5.h",
        "include/nls.h",
        "include/pathnames.h",
        "include/randutils.h",
        "include/sha1.h",
        "include/strutils.h",
    ],
    copts = [
        "-Wno-implicit-function-declaration",
        "-Wno-return-type",
    ],
    defines = ["HAVE_NANOSLEEP"],
    includes = ["include"],
)

cc_library(
    name = "uuid",
    srcs = [
        "libuuid/src/clear.c",
        "libuuid/src/compare.c",
        "libuuid/src/copy.c",
        "libuuid/src/gen_uuid.c",
        "libuuid/src/isnull.c",
        "libuuid/src/pack.c",
        "libuuid/src/parse.c",
        "libuuid/src/predefined.c",
        "libuuid/src/unpack.c",
        "libuuid/src/unparse.c",
        "libuuid/src/uuidP.h",
        "libuuid/src/uuid_time.c",
        "libuuid/src/uuidd.h",
    ],
    hdrs = [
        "libuuid/src/uuid.h",
    ],
    copts = [
        "-Wno-implicit-function-declaration",
        "-Wno-unused-parameter",
    ],
    include_prefix = "uuid",
    strip_include_prefix = "libuuid/src",
    visibility = ["//visibility:public"],
    deps = [
        ":common",
    ],
)
