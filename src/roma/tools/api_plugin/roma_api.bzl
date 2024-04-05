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

load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@rules_pkg//pkg:mappings.bzl", "pkg_files")
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")
load("@rules_proto_grpc//:defs.bzl", "bazel_build_rule_common_attrs", "filter_files")
load(
    "//src/roma/tools/api_plugin:internal/roma_api.bzl",
    "app_api_cc_protoc",
    "app_api_handler_js_protoc",
    "host_api_cc_protoc",
    "host_api_js_protoc",
    "roma_js_proto_library",
)

_closure_js_attrs = {
    "convention": "None",
    "data": [],
    "exports": [],
    "lenient": True,
    "no_closure_library": False,
    "suppress": [],
}

def _filter_files_suffix_impl(ctx):
    """Filter the files in DefaultInfo."""
    return [DefaultInfo(
        files = depset([
            file
            for target in ctx.attr.targets
            for file in target.files.to_list()
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
        "targets": attr.label_list(
            allow_empty = False,
            doc = "The source targets to filter",
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

def declare_roma_api(*, cc_protos, proto_basename, protos, host_apis = []):
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
        host_apis = host_apis,
        proto_basename = proto_basename,
        protos = protos,
    )

def js_proto_library(*, name, roma_api, **kwargs):
    """
    JS protobuf library.

    Args:
        name: target name the generated JS library
        roma_api: the roma_api struct
        **kwargs: attributes for cc_library and those common to bazel build rules
    """
    name_proto = name + "_pb"
    roma_js_proto_library(
        name = name_proto,
        roma_api = roma_api,
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
        deps = kwargs.get("deps", []) + [
            "@io_bazel_rules_closure//closure/protobuf:jspb",
        ],
        **{
            k: kwargs.get(k, v)
            for (k, v) in _closure_js_attrs.items()
        }
    )

def roma_service_js_library(*, name, roma_app_api, **kwargs):
    """
    JS service library for a Roma Application API.

    Args:
        name: name of js_binary target, basename of ancillary targets.
        roma_app_api: the roma_api struct
    """
    name_proto = name + "_proto_js_plugin"

    host_api_targets = []
    for i, host_api in enumerate(roma_app_api.host_apis):
        target_name = "{}_host_api_{}".format(name, i)
        host_api_targets.append(target_name)
        host_api_js_protoc(
            name = target_name,
            roma_host_api = host_api,
        )

    if host_api_targets:
        _filter_files_suffix(
            name = name + "_host_api_docs",
            targets = host_api_targets,
            suffixes = ["md"],
        )
        _filter_files_suffix(
            name = name + "_host_js_srcs",
            targets = host_api_targets,
            suffixes = ["js"],
        )

    app_api_handler_js_protoc(
        name = name_proto,
        roma_app_api = roma_app_api,
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
        srcs = [":{}_js_srcs".format(name)] + ([":{}_host_js_srcs".format(name)] if host_api_targets else []),
        deps = kwargs.get("deps", []) + [
            "@io_bazel_rules_closure//closure/protobuf:jspb",
        ],
        **{
            k: kwargs.get(k, v)
            for (k, v) in _closure_js_attrs.items()
        }
    )

_cc_attrs = bazel_build_rule_common_attrs + [
    "alwayslink",
    "copts",
    "defines",
    "include_prefix",
    "linkopts",
    "linkstatic",
    "local_defines",
    "nocopts",
    "strip_include_prefix",
]

def roma_host_api_cc_library(*, name, roma_host_api, **kwargs):
    """
    Top-level macro for the Roma Host API.

    Generates C++ library targets for the Roma Host API.
    This includes C++ client APIs for invoking the Roma App and protobuf helper
    functions for request/repsponse messages.

    Args:
        name: name of cc_library target, basename of ancillary targets.
        roma_host_api: the roma_api struct
        **kwargs: attributes for cc_library and those common to bazel build rules.

    Generates:
        <name>_js_host_api.md
        <name>_cpp_host_api_client_sdk.md
        <name>_roma_host.h
        <name>_native_request_handler.h

    Targets:
        <name> -- cc_library
        <name>_srcs -- c++ source files
        <name>_hdrs -- c++ header files

    Returns:
        Providers:
            - ProtoPluginInfo
            - DefaultInfo
    """

    name_proto = name + "_proto_cc_plugin"

    host_api_cc_protoc(
        name = name_proto,
        roma_host_api = roma_host_api,
    )

    # Filter files to sources and headers
    _filter_files_suffix(
        name = name + "_cc_test_srcs",
        targets = [name_proto],
        suffixes = ["_test.cc"],
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

    cc_library(
        name = name,
        hdrs = [
            ":{}_cc_hdrs".format(name),
        ],
        includes = ["."],
        deps = kwargs.get("deps", []) + roma_host_api.cc_protos + [
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/strings",
            "@com_github_grpc_grpc//:grpc++",
            Label("//src/util/status_macro:status_util"),
            Label("//src/roma/config"),
            Label("//src/roma/interface"),
            Label("//src/roma/roma_service:romav8_proto_utils"),
        ],
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
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
            Label("//src/roma/roma_service"),
        ],
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )

def roma_app_api_cc_library(*, name, roma_app_api, js_library, **kwargs):
    """
    Top-level macro for the Roma Application API.

    Generates C++ and JavaScript library targets for the Roma Application API.
    This includes C++ client APIs for invoking the Roma App and protobuf helper
    functions for request/repsponse messages.

    Args:
        name: name of cc_library target, basename of ancillary targets.
        roma_app_api: the roma_api struct
        js_library: label of the associated roma_service_js_library target.
        **kwargs: attributes for cc_library and those common to bazel build rules.

    Generates:
        <name>_js_service_sdk.md
        <name>_cpp_app_api_client_sdk.md
        <name>_roma_app.cc
        <name>_roma_app.h

    Targets:
        <name> -- cc_library
        <name>_srcs -- c++ source files
        <name>_hdrs -- c++ header files

    Returns:
        Providers:
            - ProtoPluginInfo
            - DefaultInfo
    """

    name_proto = name + "_proto_cc_plugin"

    app_api_cc_protoc(
        name = name_proto,
        roma_app_api = roma_app_api,
    )

    # Filter files to sources and headers
    _filter_files_suffix(
        name = name + "_cc_test_srcs",
        targets = [name_proto],
        suffixes = ["_test.cc"],
    )
    _filter_files_suffix(
        name = name + "_roma_app_h_tmpl",
        targets = [name_proto],
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
        content = "{}.js".format(js_library),
        out = service_h,
    )
    pkg_files(
        name = name + "_cc_service_hdrs",
        srcs = [":{}".format(service_h)],
        prefix = "docs",
    )

    cc_library(
        name = name,
        hdrs = [
            ":{}_cc_service_hdrs".format(name),
            ":{}_cc_hdrs".format(name),
        ],
        includes = ["."],
        deps = kwargs.get("deps", []) + roma_app_api.cc_protos + [
            Label("//src/roma/roma_service:romav8_app_service"),
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/strings",
        ],
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
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
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )

def roma_sdk(*, name, srcs, roma_app_api, app_api_cc_library, js_library, host_api_cc_libraries = [], **kwargs):
    """
    Top-level macro for the Roma SDK.

    Generates a bundle of SDK artifacts for the specified Roma API.

    Args:
        name: name of sdk target, basename of ancillary targets.
        srcs: label list of targets to include.
        roma_app_api: the roma_api struct.
        app_api_cc_library: label of the associated roma_app_api_cc_library target.
        js_library: label of the associated roma_service_js_library target.
        host_api_cc_libraries: labels of the associated roma_host_api_cc_library targets.
        **kwargs: attributes common to bazel build rules.

    Targets:
        <name>_doc_artifacts -- docs pkg_files
        <name> -- sdk pkg_zip
    """

    pkg_files(
        name = name + "_doc_artifacts",
        srcs = ["{}_docs".format(tgt) for tgt in (host_api_cc_libraries + [app_api_cc_library, js_library])] + (["{}_host_api_docs".format(js_library)] if roma_app_api.host_apis else []),
        prefix = "docs",
    )

    pkg_zip(
        name = name,
        srcs = srcs + [
            "{}".format(app_api_cc_library),
            "{}_cc_service_hdrs".format(app_api_cc_library),
            "{}_cc_hdrs".format(app_api_cc_library),
            ":{}_doc_artifacts".format(name),
        ],
        package_dir = "/{}".format(name),
    )
