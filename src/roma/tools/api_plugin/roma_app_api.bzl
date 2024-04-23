# Copyright 2023 Google LLC
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

"""Macro for the Roma Application API."""

load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_binary", "closure_js_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")
load("@rules_proto_grpc//:defs.bzl", "bazel_build_rule_common_attrs", "filter_files")
load(
    "//src/roma/tools/api_plugin:internal/roma_app_api.bzl",
    "app_api_cc_plugins",
    "app_api_cc_protoc",
    "app_api_js_plugins",
    "app_api_js_protoc",
    "roma_js_proto_library",
    "roma_js_proto_plugins",
)

def _filter_files_suffix_impl(ctx):
    """Filter the files in DefaultInfo."""
    return [DefaultInfo(
        files = depset([
            file
            for file in ctx.attr.target.files.to_list()
            for suffix in ctx.attr.suffixes
            if file.basename.endswith(suffix)
        ]),
    )]

_filter_files_suffix = rule(
    implementation = _filter_files_suffix_impl,
    attrs = {
        "suffixes": attr.string_list(
            doc = "The suffixes of the files to keep eg. ['h']",
            mandatory = True,
        ),
        "target": attr.label(
            doc = "The source target to filter",
            mandatory = True,
        ),
    },
)

def _expand_template_file_impl(ctx):
    ctx.actions.run_shell(
        inputs = [ctx.file.content, ctx.file.template],
        outputs = [ctx.outputs.out],
        command = """
        cp "$1" "$4"
        sed -e "/$2/{" -e "r $3" -e 'd' -e '}' -i "$4"
        """,
        arguments = [
            ctx.file.template.path,
            ctx.attr.match,
            ctx.file.content.path,
            ctx.outputs.out.path,
        ],
        mnemonic = "WrapFile",
        progress_message = "Wrapping file",
        use_default_shell_env = True,
        execution_requirements = {
            "no-cache": "1",
            "no-remote": "1",
        },
    )

_expand_template_file = rule(
    implementation = _expand_template_file_impl,
    provides = [DefaultInfo],
    attrs = {
        "content": attr.label(mandatory = True, allow_single_file = True),
        "match": attr.string(mandatory = True),
        "out": attr.output(mandatory = True),
        "template": attr.label(mandatory = True, allow_single_file = True),
    },
)

def roma_app_api(*, cc_protos, proto_basename, protos):
    """
    Creates struct for a the Roma App API as an entity.

    Args:
        cc_protos: list of proto_cc_library targets
        proto_basename: basename of the protobuf source file
        protos: list of proto_library targets

    Returns:
        struct of Roma App-related info
    """
    return struct(
        cc_protos = cc_protos,
        proto_basename = proto_basename,
        protos = protos,
    )

def js_proto_library(name, protos, proto_basename, **kwargs):
    """
    JS protobuf library.

    Args:
        name: target name the generated JS library
        protos: label list of source proto libraries
        **kwargs: attributes for cc_library and those common to bazel build rules
    """
    name_proto = name + "_proto_js_library"
    roma_js_proto_library(
        name = name_proto,
        options = {
            "@google_privacysandbox_servers_common//src/roma/tools/api_plugin:{}".format(p.name): [
                p.option.format(basename = proto_basename),
            ]
            for p in roma_js_proto_plugins
            if hasattr(p, "option")
        },
        output_mode = "NO_PREFIX",
        protos = protos,
    )
    filter_files(
        name = name + "_js_srcs",
        target = name_proto,
        extensions = ["js"],
    )
    filter_files(
        name = name + "_docs",
        target = name_proto,
        extensions = ["md"],
    )
    closure_js_library(
        name = name,
        srcs = [":{}_js_srcs".format(name)],
        convention = "NONE",
        lenient = True,
        deps = [
            "@io_bazel_rules_closure//closure/protobuf:jspb",
        ],
    )

def roma_service_js_library(name, roma_app_api):
    """
    JS service library for a Roma Application API.

    Args:
        name: name of js_binary target, basename of ancillary targets.
        roma_app_api: the roma_app_api struct
        roma_app_api: label of the roma_app_api target implemented by this JS service library.
    """
    name_proto = name + "_proto_js_plugin"

    app_api_js_protoc(
        name = name_proto,
        options = {
            "@google_privacysandbox_servers_common//src/roma/tools/api_plugin:{}".format(p.name): [
                p.option.format(basename = roma_app_api.proto_basename),
            ]
            for p in app_api_js_plugins
            if hasattr(p, "option")
        },
        output_mode = "NO_PREFIX",
        protos = roma_app_api.protos,
    )
    filter_files(
        name = name + "_js_srcs",
        target = name_proto,
        extensions = ["js"],
    )
    filter_files(
        name = name + "_docs",
        target = name_proto,
        extensions = ["md"],
    )
    closure_js_library(
        name = name + "_js_lib",
        srcs = [":{}_js_srcs".format(name)],
        convention = "NONE",
        lenient = True,
        deps = [
            "@io_bazel_rules_closure//closure/protobuf:jspb",
        ],
    )
    closure_js_binary(
        name = name,
        debug = False,
        language = "ECMASCRIPT_NEXT",
        compilation_level = "SIMPLE_OPTIMIZATIONS",
        formatting = "PRETTY_PRINT",
        deps = [":{}_js_lib".format(name)],
    )

def roma_client_cc_library(name, roma_app_api, roma_service_js_library, **kwargs):
    """
    Top-level macro for the Roma Application API.

    Generates C++ and JavaScript library targets for the Roma Application API.
    This includes C++ client APIs for invoking the Roma App and protobuf helper
    functions for request/repsponse messages.

    Args:
        name: name of cc_library target, basename of ancillary targets.
        roma_app_api: the roma_app_api struct
        roma_service_js_library: label of the associated roma_api target.
        **kwargs: attributes for cc_library and those common to bazel build rules

    Generates:
        <name>_js_server_api.md
        <name>_cpp_client_api.md
        <name>_roma_app.cc
        <name>_roma_app.h

    Targets:
        <name> -- cc_library
        <name>_srcs -- c++ source files
        <name>_hdrs -- c++ header files
        <name>_docs -- markdown docs

    Returns:
        Providers:
            - ProtoPluginInfo
            - DefaultInfo
    """

    name_proto = name + "_proto_cc_plugin"

    app_api_cc_protoc(
        name = name_proto,
        options = {
            "@google_privacysandbox_servers_common//src/roma/tools/api_plugin:{}".format(p.name): [
                p.option.format(basename = roma_app_api.proto_basename),
            ]
            for p in app_api_cc_plugins
            if hasattr(p, "option")
        },
        output_mode = "NO_PREFIX",
        protos = roma_app_api.protos,
    )

    # Filter files to sources and headers
    _filter_files_suffix(
        name = name + "_cc_test_srcs",
        target = name_proto,
        suffixes = ["_test.cc"],
    )

    _filter_files_suffix(
        name = name + "_romav8_app_pb_js_cc",
        target = name_proto,
        suffixes = ["_romav8_app_pb_js.cc"],
    )

    _filter_files_suffix(
        name = name + "_roma_app_h_tmpl",
        target = name_proto,
        suffixes = ["_roma_app.h.tmpl"],
    )

    filter_files(
        name = name + "_cc_hdrs",
        target = name_proto,
        extensions = ["h"],
    )

    filter_files(
        name = name + "_docs",
        target = name_proto,
        extensions = ["md"],
    )

    service_h = "{}_romav8_app_service.h".format(roma_app_api.proto_basename)
    _expand_template_file(
        name = "{}_romav8_app_header".format(name),
        template = ":{}_roma_app_h_tmpl".format(name),
        match = "@ROMA_APP_JSCODE@",
        content = "{}.js".format(roma_service_js_library),
        out = service_h,
    )

    cc_library(
        name = name,
        hdrs = [
            ":{}".format(service_h),
            ":{}_cc_hdrs".format(name),
        ],
        includes = ["."],
        deps = kwargs.get("deps", []) + roma_app_api.cc_protos + [
            "//src/roma/roma_service:romav8_app_service",
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/strings",
        ],
        alwayslink = kwargs.get("alwayslink"),
        copts = kwargs.get("copts"),
        defines = kwargs.get("defines"),
        include_prefix = kwargs.get("include_prefix"),
        linkopts = kwargs.get("linkopts"),
        linkstatic = kwargs.get("linkstatic"),
        local_defines = kwargs.get("local_defines"),
        nocopts = kwargs.get("nocopts"),
        strip_include_prefix = kwargs.get("strip_include_prefix"),
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # forward common bazel args
    )

    cc_test(
        name = name + "_rpc_cc_test",
        size = "small",
        srcs = [":{}_cc_test_srcs".format(name)],
        deps = kwargs.get("deps", []) + [
            ":{}".format(name),
            "@com_google_absl//absl/log:scoped_mock_log",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/synchronization",
            "@com_google_googletest//:gtest_main",
        ],
        **{
            k: v
            for (k, v) in kwargs.items()
            if k in bazel_build_rule_common_attrs
        }  # forward common bazel args
    )

    pkg_files(
        name = name + "_doc_artifacts",
        srcs = [
            ":{}_docs".format(name),
            "{}_docs".format(roma_service_js_library),
        ],
        prefix = "docs",
    )

    pkg_zip(
        name = name + "_artifacts",
        srcs = [
            ":{}".format(name),
            "{}.js".format(roma_service_js_library),
            ":{}_cc_hdrs".format(name),
            ":{}_doc_artifacts".format(name),
            service_h,
        ],
        package_dir = "/{}".format(name),
    )
