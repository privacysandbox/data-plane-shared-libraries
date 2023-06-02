load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@rules_cc//cc:defs.bzl", "cc_binary", "cc_library")

licenses(["notice"])  # MIT

filegroup(
    name = "license",
    srcs = ["COPYING"],
)

write_file(
    name = "version",
    out = "version.h",
    content = ['#define JQ_VERSION "1.4"'],
)

cc_library(
    name = "libjq",
    srcs = [
        "builtin.c",
        "builtin.h",
        "bytecode.c",
        "bytecode.h",
        "compile.c",
        "compile.h",
        "exec_stack.h",
        "execute.c",
        "jq_parser.h",
        "jq_test.c",
        "jv.c",
        "jv_alloc.c",
        "jv_alloc.h",
        "jv_aux.c",
        "jv_dtoa.c",
        "jv_dtoa.h",
        "jv_file.c",
        "jv_parse.c",
        "jv_print.c",
        "jv_unicode.c",
        "jv_unicode.h",
        "jv_utf8_tables.h",
        "lexer.c",
        "lexer.h",
        "libm.h",
        "locfile.c",
        "locfile.h",
        "opcode_list.h",
        "parser.c",
        "parser.h",
    ],
    hdrs = [
        "jq.h",
        "jv.h",
        ":version.h",
    ],
    copts = [
        "-Wno-unused-function",
        "-Wno-unused-variable",
    ],
    includes = ["."],
)

cc_binary(
    name = "jq",
    srcs = ["main.c"],
    copts = ["-Wno-unused-function"],
    linkopts = ["-lm"],
    visibility = ["//visibility:public"],
    deps = [":libjq"],
)
