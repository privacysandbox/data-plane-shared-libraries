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

load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library", "cc_test")

package(default_visibility = ["//visibility:public"])

genrule(
    name = "cap_names_list_h",
    srcs = ["libcap/include/uapi/linux/capability.h"],
    outs = ["cap_names.list.h"],
    # Use the same logic as libcap/Makefile
    cmd = """
    perl -e 'while ($$l=<>) {
      if ($$l =~ /^\\#define[ \\t](CAP[_A-Z]+)[ \\t]+([0-9]+)\\s+$$/) {
        $$tok=$$1; $$val=$$2; $$tok =~ tr/A-Z/a-z/;
        print \"{\\\"$$tok\\\",$$val},\\n\"; } }' $< \
    | fgrep -v 0x > $@
    """,
)

cc_library(
    name = "makenames_textual_hdrs",
    textual_hdrs = [
        "cap_names.list.h",
        "libcap/include/uapi/linux/capability.h",
    ],
    visibility = ["//visibility:private"],
)

cc_binary(
    name = "makenames",
    srcs = [
        "libcap/_makenames.c",
        "libcap/include/sys/capability.h",
    ],
    includes = [
        "libcap/..",
        "libcap/include",
        "libcap/include/uapi",
    ],
    deps = [
        ":makenames_textual_hdrs",
    ],
)

genrule(
    name = "cap_names_h",
    outs = ["libcap/cap_names.h"],
    cmd = "mkdir -p libcap && $(location makenames) > $@ || { rm -f $@; false; }",
    tools = [":makenames"],
)

cc_library(
    name = "libcap",
    srcs = [
        "libcap/cap_alloc.c",
        "libcap/cap_extint.c",
        "libcap/cap_file.c",
        "libcap/cap_flag.c",
        "libcap/cap_names.h",
        "libcap/cap_proc.c",
        "libcap/cap_text.c",
        "libcap/libcap.h",
    ],
    copts = [
        "-Wno-tautological-compare",
        "-Wno-unused-result",
    ],
    includes = [
        "libcap",
        "libcap/include",
        "libcap/include/uapi",
    ],
    textual_hdrs = [
        "libcap/include/sys/capability.h",
        "libcap/include/sys/securebits.h",
        "libcap/include/uapi/linux/capability.h",
        "libcap/include/uapi/linux/prctl.h",
        "libcap/include/uapi/linux/securebits.h",
    ],
)

cc_test(
    name = "cap_test",
    size = "small",
    srcs = [
        "libcap/cap_names.h",
        "libcap/cap_test.c",
        "libcap/libcap.h",
    ],
    deps = [
        ":libcap",
    ],
)

cc_test(
    name = "libcap_test",
    size = "small",
    deps = [
        ":libcap",
        "@com_google_googletest//:gtest_main",
    ],
)
