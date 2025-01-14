# Copyright 2025 Google LLC
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
load("@rules_license//rules:license.bzl", "license")

package(
    default_applicable_licenses = [":license"],
    default_visibility = ["//visibility:public"],
)

license(
    name = "license",
    package_name = "libseccomp",
)

licenses(["restricted"])

exports_files(["LICENSE"])

genrule(
    name = "seccomp_h",
    srcs = [
        "include/seccomp.h.in",
    ],
    outs = [
        "include/seccomp.h",
    ],
    cmd = "sed 's/@VERSION_MAJOR@/2/g; s/@VERSION_MINOR@/5/g; s/@VERSION_MICRO@/5/g' $< > $@",
)

cc_library(
    name = "util",
    srcs = [
        "tools/util.c",
    ],
    hdrs = [
        "tools/util.h",
    ],
    copts = [
        "-w",
        "-DHAVE_CONFIG_H",
    ],
)

cc_library(
    name = "libseccomp",
    srcs =
        [
            "src/api.c",
            "src/arch.c",
            "src/arch-aarch64.c",
            "src/arch-arm.c",
            "src/arch-mips.c",
            "src/arch-mips64.c",
            "src/arch-mips64n32.c",
            "src/arch-parisc.c",
            "src/arch-parisc64.c",
            "src/arch-ppc.c",
            "src/arch-ppc64.c",
            "src/arch-riscv64.c",
            "src/arch-s390.c",
            "src/arch-s390x.c",
            "src/arch-syscall-dump.c",
            "src/arch-x32.c",
            "src/arch-x86.c",
            "src/arch-x86_64.c",
            "src/db.c",
            "src/gen_bpf.c",
            "src/gen_pfc.c",
            "src/hash.c",
            "src/helper.c",
            "src/syscalls.c",
            "src/syscalls.perf.c",
            "src/system.c",
        ],
    copts = [
        "-w",
        "-DHAVE_CONFIG_H",
    ],
    includes = [
        "include",
    ],
    textual_hdrs =
        [
            "include/seccomp.h",
            "include/seccomp-syscalls.h",
            "configure.h",
            "src/arch.h",
            "src/arch-aarch64.h",
            "src/arch-arm.h",
            "src/arch-mips.h",
            "src/arch-mips64.h",
            "src/arch-mips64n32.h",
            "src/arch-parisc.h",
            "src/arch-parisc64.h",
            "src/arch-ppc.h",
            "src/arch-ppc64.h",
            "src/arch-riscv64.h",
            "src/arch-s390.h",
            "src/arch-s390x.h",
            "src/arch-x32.h",
            "src/arch-x86.h",
            "src/arch-x86_64.h",
            "src/db.h",
            "src/gen_bpf.h",
            "src/gen_pfc.h",
            "src/hash.h",
            "src/helper.h",
            "src/syscalls.h",
            "src/system.h",
        ],
    deps = [":util"],
)
