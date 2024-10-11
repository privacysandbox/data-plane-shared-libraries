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

load("@bazel_skylib//rules:copy_file.bzl", "copy_file")
load("@bazel_skylib//rules:write_file.bzl", "write_file")
load("@com_google_googleapis_imports//:imports.bzl", "cc_proto_library")
load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")
load("@rules_buf//buf:defs.bzl", "buf_lint_test")
load("@rules_cc//cc:defs.bzl", "cc_library", "cc_test")
load("@rules_oci//oci:defs.bzl", "oci_image", "oci_load")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")
load("@rules_proto//proto:defs.bzl", "proto_library")
load("@rules_proto_grpc//:defs.bzl", "bazel_build_rule_common_attrs", "filter_files")
load(
    "//src/roma/tools/api_plugin:internal/roma_api.bzl",
    "app_api_cc_protoc",
    "app_api_handler_js_protoc",
    "byob_udf_protospec",
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

def roma_v8_app_api_cc_library(*, name, roma_app_api, js_library, **kwargs):
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
    _filter_files_suffix(
        name = name + "_cc_hdrs",
        targets = [name_proto],
        suffixes = ["roma_app_service.h"],
    )
    _filter_files_suffix(
        name = name + "_pb_h",
        targets = [name_proto],
        suffixes = [".pb.h"],
    )

    # duplicate $name.pb.h as $name_udf_interface.pb.h for alignment with Roma BYOB
    copy_file(
        name = name + "_udf_interface_pb_h",
        src = ":{}_pb_h".format(name),
        out = "{}_udf_interface.pb.h".format(roma_app_api.proto_basename),
        visibility = ["//visibility:public"],
    )
    _filter_files_suffix(
        name = name + "_docs",
        targets = [name_proto],
        suffixes = ["cc_v8_app_api_client_sdk.md"],
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
    )
    cc_library(
        name = name,
        hdrs = [
            ":{}_cc_service_hdrs".format(name),
            ":{}_cc_hdrs".format(name),
            ":{}_udf_interface_pb_h".format(name),
        ],
        includes = ["."],
        deps = kwargs.get("deps", []) + roma_app_api.cc_protos + [
            Label("//src/roma/roma_service:romav8_app_service"),
            "@com_google_absl//absl/functional:any_invocable",
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

def roma_byob_app_api_cc_library(*, name, roma_app_api, **kwargs):
    """
    Top-level macro for the Roma BYOB Application API.

    Generates C++ and JavaScript library targets for the Roma BYOB Application API.
    This includes C++ client APIs for invoking the Roma App and protobuf helper
    functions for request/repsponse messages.

    Args:
        name: name of cc_library target, basename of ancillary targets.
        roma_app_api: the roma_api struct
        **kwargs: attributes for cc_library and those common to bazel build rules.

    Generates:
        <name>_cc_byob_app_api_client_sdk.md
        <name>_roma_app_service.h
        <name>_roma_byob_app_service.h

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

    _filter_files_suffix(
        name = "{}_roma_byob_app_header".format(name),
        targets = [name_proto],
        suffixes = ["app_service.h"],
    )
    _filter_files_suffix(
        name = name + "_docs",
        targets = [name_proto],
        suffixes = ["cc_byob_app_api_client_sdk.md"],
    )
    cc_library(
        name = name,
        hdrs = ["{}_roma_byob_app_header".format(name)],
        includes = ["."],
        deps = kwargs.get("deps", []) + roma_app_api.cc_protos + [
            Label("//src/roma/roma_service:romav8_app_service"),
            Label("//src/roma/byob/interface:roma_service"),
            Label("//src/roma/byob/config"),
            "@com_google_absl//absl/functional:any_invocable",
            "@com_google_absl//absl/status",
            "@com_google_absl//absl/strings",
        ],
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )

def romav8_image(*, name, cc_binary, repo_tag):
    """
    Creates a Roma V8 container image.

    Args:
        name: name of sdk target, basename of ancillary targets.
        cc_binary: label of cc_binary target to include.
        repo_tag: tag for generated OCI image.

    Targets:
        <name>_image -- the oci_image target
        <name>_tarball.tar -- tarfile of the OCI image
    """
    _roma_image(name = name, cc_binary = cc_binary, repo_tag = repo_tag, type = "v8")

def _roma_image(*, name, cc_binary, repo_tag, type):
    type_str = "roma" + type
    path = "/usr/bin"
    pkg_files(
        name = "{}_{}_cli_execs".format(name, type_str),
        srcs = [
            cc_binary,
        ],
        attributes = pkg_attributes(mode = "0555"),
        prefix = path,
    )
    pkg_tar(
        name = "{}_{}_cli_tar".format(name, type_str),
        srcs = [
            ":{}_{}_cli_execs".format(name, type_str),
        ],
    )
    oci_image(
        name = "{}_image".format(name),
        base = select({
            "@platforms//cpu:aarch64": "@runtime-debian-debug-root-arm64",
            "@platforms//cpu:x86_64": "@runtime-debian-debug-root-amd64",
        }),
        entrypoint = ["{}/{}".format(path, cc_binary.name)],
        tars = [
            ":{}_{}_cli_tar".format(name, type_str),
        ],
    )
    oci_load(
        name = "{}_tarball".format(name),
        image = ":{}_image".format(name),
        repo_tags = ["privacy_sandbox/udf_sdk/{}:{}".format(name, repo_tag)],
    )
    native.filegroup(
        name = "{}_tarball.tar".format(name),
        srcs = [":{}_tarball".format(name)],
        output_group = "tarball",
    )

def roma_integrator_docs(*, name, app_api_cc_library, host_api_cc_libraries = [], **kwargs):
    """
    Generates a bundle of docs for C++ integrators of the specified Roma API.

    Args:
        name: name of sdk target, basename of ancillary targets.
        app_api_cc_library: label of the associated roma_app_api_cc_library target.
        host_api_cc_libraries: labels of the associated roma_host_api_cc_library targets.
        **kwargs: attributes common to bazel build rules.

    Targets:
        <name>_doc_artifacts -- docs pkg_files
        <name> -- docs pkg_zip
    """
    pkg_files(
        name = name + "_doc_artifacts",
        srcs = ["{}_docs".format(lib) for lib in host_api_cc_libraries + [app_api_cc_library]],
    )
    pkg_zip(
        name = name,
        srcs = [
            ":{}_doc_artifacts".format(name),
        ],
        package_dir = "/{}".format(name),
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )

def roma_v8_sdk(*, name, srcs, roma_app_api, app_api_cc_library, js_library, repo_tag = "v1", **kwargs):
    """
    Top-level macro for the Roma SDK.

    Generates a bundle of SDK artifacts for the specified Roma API.

    Args:
        name: name of sdk target, basename of ancillary targets.
        srcs: label list of targets to include.
        roma_app_api: the roma_api struct.
        app_api_cc_library: label of the associated roma_app_api_cc_library target.
        js_library: label of the associated roma_service_js_library target.
        repo_tag: tag for generated OCI images.
        host_api_cc_libraries: labels of the associated roma_host_api_cc_library targets.
        **kwargs: attributes common to bazel build rules.

    Targets:
        <name>_doc_artifacts -- docs pkg_files
        <name>_doc_tools_artifacts -- CLI tools docs pkg_files
        <name>_doc_udf_artifacts -- js_library docs pkg_files
        <name> -- sdk pkg_zip
    """
    pkg_files(
        name = name + "_doc_tools_artifacts",
        srcs = [
            Label("//docs/roma:v8/sdk/docs/tools/shell_cli.md"),
            Label("//docs/roma:v8/sdk/docs/tools/udf_benchmark_cli.md"),
            Label("//docs/roma:v8/sdk/docs/tools/logging.md"),
        ],
        prefix = "docs/tools",
    )
    pkg_files(
        name = name + "_doc_artifacts",
        srcs = [Label("//docs/roma:v8/sdk/docs/Guide to the SDK.md")],
        prefix = "docs",
    )
    pkg_files(
        name = name + "_doc_udf_artifacts",
        srcs = ["{}_docs".format(js_library)] + (["{}_host_api_docs".format(js_library)] if roma_app_api.host_apis else []),
        prefix = "docs/udf",
    )

    romav8_image(
        name = name + "_roma_shell",
        cc_binary = Label("//src/roma/tools/v8_cli:roma_shell"),
        repo_tag = repo_tag,
    )
    romav8_image(
        name = name + "_roma_benchmark",
        cc_binary = Label("//src/roma/tools/v8_cli:roma_benchmark"),
        repo_tag = repo_tag,
    )

    pkg_files(
        name = name + "_tools_artifacts",
        srcs = [
            ":{}_roma_shell_tarball.tar".format(name),
            ":{}_roma_benchmark_tarball.tar".format(name),
        ],
        # oci 2.0 rules generate all tarballs the same name "tarball.tar", and we need to rename them.
        # Without this rename, we'll get the following error:
        # "After renames, multiple sources (at least {0}, {1}) map to the same destination. Consider adjusting strip_prefix and/or renames error".
        renames = {
            ":{}_roma_shell_tarball.tar".format(name): "{}_roma_shell_tarball.tar".format(name),
            ":{}_roma_benchmark_tarball.tar".format(name): "{}_roma_benchmark_tarball.tar".format(name),
        },
        prefix = "tools",
    )
    pkg_zip(
        name = name,
        srcs = srcs + [
            "{}".format(app_api_cc_library),
            "{}_cc_service_hdrs".format(app_api_cc_library),
            "{}_cc_hdrs".format(app_api_cc_library),
            ":{}_doc_artifacts".format(name),
            ":{}_tools_artifacts".format(name),
            ":{}_doc_tools_artifacts".format(name),
            ":{}_doc_udf_artifacts".format(name),
        ],
        package_dir = "/{}".format(name),
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )

def declare_doc(*, doc, target_filename = "", target_subdir = ""):
    """
    Creates struct for a the Roma App API as an entity.

    Args:
        doc: label of the document
        target_subdir: target subdirectory of the docs tree

    Returns:
        struct of Roma App-related info
    """
    return struct(
        doc = doc,
        target_subdir = target_subdir,
        target_filename = target_filename,
    )

_default_guide_intro = """
This SDK provides the documentation and the binary specification for
developers of user-defined functions (UDFs).
"""

def sdk_guide_doc(*, name, intro_text = _default_guide_intro):
    """
    Creates a file target for the Guide to the SDK markdown doc.

    Args:
        intro_text: string containing markdown text for the introduction,
                    inserted after the title
    """
    title = "# Guide to the SDK"
    udf_spec_runtime_text = """
## The UDF specification

The UDF API specification defines the request and response structures for this
UDF. This API forms the high-level protocol for the UDF, and is specific to each
SDK. The spec is defined using [Protocol Buffers (protobuf)](https://protobuf.dev/),
and is included in the SDK as [udf_interface.proto](../specs/udf_interface.proto).

## The UDF runtime

The UDF binaries are executed within a sandboxed environment. Details on this
runtime environment and its requirements can be found in [Execution Environment and Interface](udf/Execution%20Environment%20and%20Interface.md).
This includes details on the Linux container images used for the runtime, which
is relevant for any UDF binaries that depend on libc or other Linux shared
libraries.

The UDF runtime also defines a communications protocol which must be
implemented by each UDF. This protocol defines the encoding format used for all
communications between the UDF runtime and the UDF, and is agnostic of the
high-level protobuf UDF spec. Details of the communications protocol are
documented in [Communication Interface](udf/Communication%20Interface.md).
"""

    write_file(
        name = name + "_file",
        out = name,
        content = [
            title,
            intro_text,
            udf_spec_runtime_text,
        ],
    )

def sdk_runtime_doc(*, name):
    text = """
# Execution environment and interface

## Execution environment and libraries

The UDF will execute in a container with the base image being a
[Google distroless container image](https://github.com/GoogleContainerTools/distroless). The
distroless images currently supported are:

| ARCHITECTURE | IMAGE             | TAG           | HASH (SHA256)                                                      |
| ------------ | ------------------------------ | ------------- | ------------------------------------------------------------------ |
| AMD64        | gcr.io/distroless/cc-debian11 | nonroot-amd64 | `5a9e854bab8498a61a66b2cfa4e76e009111d09cb23a353aaa8d926e29a653d9` |
| ARM64        | gcr.io/distroless/cc-debian11 | nonroot-arm64 | `3122cd55375a0a9f32e56a18ccd07572aeed5682421432701a03c335ab79c650` |

These images contain a minimal Linux, glibc runtime for "mostly-statically compiled" languages like
C/C++, GO, Rust, D, among others.

Images can be downloaded using the IMAGE and HASH values from the table above. To retrieve the image
using docker:

```shell
docker pull IMAGE@sha256:HASH
```

UDFs must be provided as a single, self-contained executable file. The executable may depend on
shared libraries contained within the base distroless image, for example, `glibc`, `libgcc1` and its
dependencies.

Build toolchain base image: `ubuntu:20.04`

## Command-line flags

Command line flag are used to pass the file descriptor (fd) over which the UDF can communicate. The
first positional argument to the UDF is the fd for communication.

For details about how to use the passed flag(s), see
[communication interface](Communication%20Interface.md) doc and
[example UDFs](https://github.com/privacysandbox/data-plane-shared-libraries/tree/e5d685e2d07b4535b650e4f44f8473e187408fc6/src/roma/byob/example).

## gVisor

gVisor supports a large subset of Linux syscalls; some syscalls may have a partial implementation.
Refer to gVisor's [list of supported syscalls](syscalls.md).
"""
    write_file(
        name = name + "_file",
        out = name,
        content = [text],
    )

def roma_byob_sdk(
        *,
        name,
        roma_app_api,
        extra_docs = [],
        guide_intro_text = _default_guide_intro,
        **kwargs):
    """
    Top-level macro for the Roma BYOB SDK.

    Generates a bundle of SDK artifacts for the specified Roma BYOB API.

    Args:
        name: name of sdk target, basename of ancillary targets.
        roma_app_api: the roma_api struct.
        extra_docs: a list of declare_doc-created structs
        guide_intro_text: string containing markdown text for the guide introduction
        **kwargs: attributes common to bazel build rules.

    Targets:
        <name> -- sdk pkg_zip
        <name>_doc_artifacts -- docs pkg_files
        <name>.proto -- proto spec
        <name>_proto -- proto_library
        <name>_cc_proto -- cc_proto_library
        <name>_roma_cc_lib -- roma_byob_app_api_cc_library
    """
    byob_udf_protospec(
        name = name + ".proto",
        roma_app_api = roma_app_api,
    )
    proto_library(
        name = name + "_proto",
        srcs = [":{}.proto".format(name)],
    )
    cc_proto_library(
        name = name + "_cc_proto",
        deps = [":{}_proto".format(name)],
        **{k: v for (k, v) in kwargs.items() if k in bazel_build_rule_common_attrs}
    )
    roma_byob_app_api_cc_library(
        name = name + "_roma_cc_lib",
        roma_app_api = roma_app_api,
        tags = [
            "noasan",
            "notsan",
        ],
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )
    buf_lint_test(
        name = name + "_proto_lint",
        size = "small",
        config = Label("//src:buf.yaml"),
        targets = [":{}_proto".format(name)],
    )
    pkg_files(
        name = name + "_specs",
        srcs = [":{}.proto".format(name)],
        prefix = "specs",
        renames = {
            ":{}.proto".format(name): "udf_interface.proto",
        },
    )
    sdk_guide_doc(
        name = name + "_guide_md",
        intro_text = guide_intro_text,
    )
    sdk_runtime_doc(name = name + "_runtime_md")
    copy_file(
        name = name + "_syscalls_md",
        src = select({
            "@platforms//cpu:aarch64": Label("//docs/roma:byob/sdk/docs/udf/arm64-syscalls.md"),
            "@platforms//cpu:x86_64": Label("//docs/roma:byob/sdk/docs/udf/amd64-syscalls.md"),
        }),
        out = "{}_syscalls.md".format(name),
    )
    docs = [
        declare_doc(
            doc = ":{}_guide_md".format(name),
            target_filename = "Guide to the SDK.md",
        ),
        # Uncomment when UDF Interface Specifications.md has been written.
        # declare_doc(
        #     doc = Label("//docs/roma:byob/sdk/docs/udf/UDF Interface Specifications.md"),
        #     target_subdir = "udf",
        # ),
        declare_doc(
            doc = ":{}_runtime_md".format(name),
            target_filename = "Execution Environment and Interface.md",
            target_subdir = "udf",
        ),
        declare_doc(
            doc = Label("//docs/roma:byob/sdk/docs/udf/Communication Interface.md"),
            target_subdir = "udf",
        ),
        declare_doc(
            doc = ":{}_syscalls.md".format(name),
            target_filename = "syscalls.md",
            target_subdir = "udf",
        ),
    ] + extra_docs

    docs_subdirs = {d.target_subdir: 0 for d in docs}.keys()

    [
        pkg_files(
            name = "{}_{}_doc_artifacts".format(name, hash(dir)),
            srcs = [d.doc for d in docs if d.target_subdir == dir],
            prefix = "docs/{}".format(dir),
            renames = {
                d.doc: d.target_filename
                for d in docs
                if d.target_subdir == dir and d.target_filename
            },
        )
        for dir in docs_subdirs
    ]

    pkg_zip(
        name = name,
        srcs = kwargs.get("srcs", []) + [
            "{}_specs".format(name),
        ] + [
            ":{}_{}_doc_artifacts".format(name, hash(dir))
            for dir in docs_subdirs
        ],
        package_dir = "/{}".format(name),
        **{k: v for (k, v) in kwargs.items() if k in _cc_attrs}
    )
