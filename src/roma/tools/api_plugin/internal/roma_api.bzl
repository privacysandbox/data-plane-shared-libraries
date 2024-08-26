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

"""Helper rules and protoc plugin structs for the Roma Application API."""

load(
    "@rules_proto_grpc//:defs.bzl",
    "ProtoPluginInfo",
    "proto_compile_impl",
    grpc_proto_compile_attrs = "proto_compile_attrs",
)

# the template_dir_name must be a valid string value for unix directories, the
# path does not need to exist -- the golang plugin and the plugin options both
# use this string value through this template_dir_name variable
template_dir_name = "roma-api-templates"

def _get_template_options(suffix, template_file, sub_dir):
    return "{tmpl_dir}/tmpl/{sub_dir}/{tmpl},{basename}{suffix}".format(
        tmpl_dir = template_dir_name,
        basename = "{basename}",
        suffix = suffix,
        tmpl = template_file,
        sub_dir = sub_dir,
    )

def _get_proto_compile_attrs(plugins):
    return dict(
        # override the default output mode
        {k: v for k, v in grpc_proto_compile_attrs.items() if k not in ("output_mode")},
        output_mode = attr.string(
            default = "NO_PREFIX",
            values = ["PREFIXED", "NO_PREFIX", "NO_PREFIX_FLAT"],
            doc = "The output mode for the target. NO_PREFIX will output files directly to the current package",
        ),
        _plugins = attr.label_list(
            providers = [ProtoPluginInfo],
            default = [Label("@google_privacysandbox_servers_common//src/roma/tools/api_plugin:{}".format(plugin.name)) for plugin in plugins],
        ),
    )

def _roma_api_protoc(*, name, protoc_rule, plugins, roma_api, **kwargs):
    protoc_rule(
        name = name,
        options = {
            "@google_privacysandbox_servers_common//src/roma/tools/api_plugin:{}".format(p.name): [
                p.option.format(basename = roma_api.proto_basename),
            ]
            for p in plugins
            if hasattr(p, "option")
        },
        protos = roma_api.protos,
        **kwargs
    )

_cc_app_template_plugins = [
    struct(
        name = "roma_app_api_cc_plugin{}".format(i),
        option = _get_template_options(plugin.suffix, plugin.template_file, plugin.sub_directory),
        outputs = ["{basename}" + plugin.suffix],
        tool = "//src/roma/tools/api_plugin:roma_api_plugin",
    )
    for i, plugin in enumerate([
        struct(
            template_file = "md_cpp_client_sdk.tmpl",
            suffix = "_cc_v8_app_api_client_sdk.md",
            sub_directory = "v8/app",
        ),
        struct(
            template_file = "cpp_test_romav8.tmpl",
            suffix = "_romav8_app_test.cc",
            sub_directory = "v8/app",
        ),
        struct(
            template_file = "hpp_romav8.tmpl",
            suffix = "_roma_app.h.tmpl",
            sub_directory = "v8/app",
        ),
        struct(
            template_file = "hpp_roma.tmpl",
            suffix = "_roma_app_service.h",
            sub_directory = "common/app",
        ),
        struct(
            template_file = "hpp_roma_byob.tmpl",
            suffix = "_roma_byob_app_service.h",
            sub_directory = "byob/app",
        ),
        struct(
            template_file = "md_cpp_client_sdk.tmpl",
            suffix = "_cc_byob_app_api_client_sdk.md",
            sub_directory = "byob/app",
        ),
    ])
]

_cc_protobuf_plugins = [
    struct(
        name = "cpp_proto_plugin",
        exclusions = [
            "google/protobuf",
        ],
        outputs = [
            "{protopath}.pb.h",
            "{protopath}.pb.cc",
        ],
        protoc_plugin_name = "cpp",
        tool = "@com_google_protobuf//:protoc",
    ),
]

_app_api_cc_plugins = _cc_protobuf_plugins + _cc_app_template_plugins

_app_api_cc_protoc = rule(
    implementation = proto_compile_impl,
    attrs = _get_proto_compile_attrs(_app_api_cc_plugins),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

def app_api_cc_protoc(*, name, roma_app_api, **kwargs):
    _roma_api_protoc(
        name = name,
        protoc_rule = _app_api_cc_protoc,
        plugins = _app_api_cc_plugins,
        roma_api = roma_app_api,
        **kwargs
    )

_app_api_handler_js_plugins = [
    struct(
        name = "roma_app_api_js_plugin{}".format(i),
        exclusions = [],
        option = _get_template_options(plugin.suffix, plugin.template_file, plugin.sub_directory),
        outputs = ["{basename}" + plugin.suffix],
        tool = "//src/roma/tools/api_plugin:roma_api_plugin",
    )
    for i, plugin in enumerate([
        struct(
            template_file = "md_js_service_sdk.tmpl",
            suffix = "_js_service_sdk.md",
            sub_directory = "v8/app",
        ),
        struct(
            template_file = "js_pb_helpers.tmpl",
            suffix = "_pb_helpers.js",
            sub_directory = "v8",
        ),
        struct(
            template_file = "js_service_handlers.tmpl",
            suffix = "_service_handlers.js",
            sub_directory = "v8/app",
        ),
        struct(
            template_file = "js_service_handlers_extern.tmpl",
            suffix = "_service_handlers_extern.js",
            sub_directory = "v8/app",
        ),
    ])
]

_app_api_handler_js_protoc = rule(
    implementation = proto_compile_impl,
    attrs = _get_proto_compile_attrs(_app_api_handler_js_plugins),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

def app_api_handler_js_protoc(*, name, roma_app_api, **kwargs):
    _roma_api_protoc(
        name = name,
        protoc_rule = _app_api_handler_js_protoc,
        plugins = _app_api_handler_js_plugins,
        roma_api = roma_app_api,
        **kwargs
    )

_cc_host_template_plugins = [
    struct(
        name = "roma_host_api_cc_plugin{}".format(i),
        option = _get_template_options(plugin.suffix, plugin.template_file, plugin.sub_directory),
        outputs = ["{basename}" + plugin.suffix],
        tool = "//src/roma/tools/api_plugin:roma_api_plugin",
    )
    for i, plugin in enumerate([
        struct(
            template_file = "md_cpp_client_sdk.tmpl",
            suffix = "_cc_client_sdk.md",
            sub_directory = "v8/host",
        ),
        struct(
            template_file = "cpp_test_romav8.tmpl",
            suffix = "_romav8_test.cc",
            sub_directory = "v8/host",
        ),
        struct(
            template_file = "hpp_romav8.tmpl",
            suffix = "_roma_host.h",
            sub_directory = "v8/host",
        ),
        struct(
            template_file = "cpp_handle_native_request.tmpl",
            suffix = "_native_request_handler.h",
            sub_directory = "v8/host",
        ),
    ])
]

_host_api_cc_plugins = _cc_protobuf_plugins + _cc_host_template_plugins

_host_api_cc_protoc = rule(
    implementation = proto_compile_impl,
    attrs = _get_proto_compile_attrs(_host_api_cc_plugins),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

def host_api_cc_protoc(*, name, roma_host_api, **kwargs):
    _roma_api_protoc(
        name = name,
        protoc_rule = _host_api_cc_protoc,
        plugins = _host_api_cc_plugins,
        roma_api = roma_host_api,
        **kwargs
    )

_host_api_js_plugins = [
    struct(
        name = "roma_host_api_js_plugin{}".format(i),
        exclusions = [],
        option = _get_template_options(plugin.suffix, plugin.template_file, plugin.sub_directory),
        outputs = ["{basename}" + plugin.suffix],
        tool = "//src/roma/tools/api_plugin:roma_api_plugin",
    )
    for i, plugin in enumerate([
        struct(
            template_file = "md_js_service_sdk.tmpl",
            suffix = "_js_service_sdk.md",
            sub_directory = "v8/host",
        ),
        struct(
            template_file = "js_pb_helpers.tmpl",
            suffix = "_pb_helpers.js",
            sub_directory = "v8",
        ),
        struct(
            template_file = "js_callback_wrappers.tmpl",
            suffix = "_callback_wrappers.js",
            sub_directory = "v8/host",
        ),
    ])
]

_host_api_js_protoc = rule(
    implementation = proto_compile_impl,
    attrs = _get_proto_compile_attrs(_host_api_js_plugins),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

def host_api_js_protoc(*, name, roma_host_api, **kwargs):
    _roma_api_protoc(
        name = name,
        protoc_rule = _host_api_js_protoc,
        plugins = _host_api_js_plugins,
        roma_api = roma_host_api,
        **kwargs
    )

_protobuf_js_plugins = [
    struct(
        name = "js",
        exclusions = [],
        option = ",".join([
            "binary",
            "import_style=closure",
            "library={basename}_pb",
        ]),
        outputs = ["{basename}_pb.js"],
        tool = "@protocolbuffers_protobuf_javascript//generator:protoc-gen-js",
    ),
]

_roma_js_proto_library = rule(
    implementation = proto_compile_impl,
    attrs = _get_proto_compile_attrs(_protobuf_js_plugins),
    toolchains = [str(Label("@rules_proto_grpc//protobuf:toolchain_type"))],
)

def roma_js_proto_library(*, name, roma_api, **kwargs):
    _roma_api_protoc(
        name = name,
        protoc_rule = _roma_js_proto_library,
        plugins = _protobuf_js_plugins,
        roma_api = roma_api,
        **kwargs
    )

def get_all_roma_api_plugins():
    return _app_api_cc_plugins + _app_api_handler_js_plugins + _cc_host_template_plugins + _host_api_js_plugins + _protobuf_js_plugins
