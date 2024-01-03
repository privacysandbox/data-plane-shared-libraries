# Copyright 2023 Google LLC
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

load("@emsdk//emscripten_toolchain:wasm_rules.bzl", "wasm_cc_binary")
load("@rules_cc//cc:defs.bzl", "cc_binary")

_EMSCRIPTEN_LINKOPTS = [
    # Enable embind
    "--bind",
    # no main function
    "--no-entry",
    # optimization
    "-O3",
    # Do not use closure. We probably want to use closure eventually.
    "--closure=0",
    "-s MODULARIZE=1",
    "-s EXPORT_NAME=wasmModule",
    # Disable the filesystem.
    "-s FILESYSTEM=0",
    # Use environment with fewer "extra" features.
    "-s ENVIRONMENT=shell",
]

_STANDALONE_WASM_LINKOPTS = [
    "-Oz",
    "-DNDEBUG",
    "-sEXPORTED_FUNCTIONS=\"['_Handler']\"",
    "-Wl",
    "--no-entry",
    "-s STANDALONE_WASM",
]

def inline_wasm_cc_binary(
        name,
        srcs,
        outputs,
        deps = [],
        tags = [],
        linkopts = [],
        standalone = False):
    """Exposes a wasm_cc_binary target of the specified `name`.

    This wasm_cc_binary is generated from a cc_binary with the specified `srcs`.

    Example usage:
        inline_wasm_cc_binary(
            name = "cpp_wasm_hello_world_example",
            srcs = ["hello_world.cc"],
            outputs = [
                "hello_world.wasm",
            ],
            deps = [
              "hello_world_deps.cc",
            ],
            standalone = True,
        )

    Args:
        name: BUILD target name
        srcs: Names of the files that will be compiled to WASM
        outputs: Names of the files that wasm_cc_binary will output
        deps: List of other libraries to be linked in to the cc_binary target
        tags: tags to propagate to rules
        standalone: Boolean for if inline or standalone wasm should be generated.
        linkopts: Additional linkopts for the cc_target
    """

    cc_binary(
        name = "{}_cc".format(name),
        srcs = srcs,
        deps = deps,
        linkopts = linkopts + (_STANDALONE_WASM_LINKOPTS if standalone else _EMSCRIPTEN_LINKOPTS),
        # This target won't build successfully on its own because of missing emscripten
        # headers etc. Therefore, we hide it from wildcards.
        tags = ["manual"],
        visibility = ["//visibility:private"],
    )

    # Generate WASM + JS using emscripten
    wasm_cc_binary(
        name = name,
        cc_target = "{}_cc".format(name),
        outputs = outputs,
        visibility = ["//visibility:public"],
        tags = tags,
    )

def inline_wasm_udf_js(
        name,
        wasm_binary,
        glue_js,
        tags = ["manual"]):
    """Generate a JS file containing inline WASM and glue JS file.

    Performs the following steps:
    1. Takes a wasm_binary and inlines it to JS.
    2. The inlined wasm + glue JS is used to generate the final JS file.

    Example usage:
        inline_wasm_udf_js(
            name = "hello_udf_js",
            wasm_binary = "hello.wasm",
            glue_js = "hello.js",
        )

    Args:
        name: BUILD target name
        wasm_binary: WASM binary
        glue_js: Javascript glue code
        tags: tags to propagate to rules
    """
    get_module_js = """async function getModule(){
            var Module = {
            instantiateWasm: function (imports, successCallback) {
                var module = new WebAssembly.Module(wasm_array);
                var instance = new WebAssembly.Instance(module, imports);
                Module.testWasmInstantiationSucceeded = 1;
                successCallback(instance);
                return instance.exports;
            }
            };
            return await wasmModule(Module);
        }"""

    native.genrule(
        name = name,
        srcs = [wasm_binary, glue_js],
        outs = ["{}_generated.js".format(name)],
        cmd_bash = """WASM_HEX=$$(
hexdump -v -e '1/1 "0x%02x,"' $(location {wasm_binary})
)
cat << EOF > $@
let wasm_array = new Uint8Array([$$WASM_HEX]);
$$(cat $(location {glue_js}))
{module_js}
EOF""".format(
            wasm_binary = wasm_binary,
            glue_js = glue_js,
            module_js = get_module_js,
        ),
        visibility = ["//visibility:public"],
        tags = tags,
    )

def cc_inline_wasm_udf_js(
        name,
        srcs,
        tags = ["manual"]):
    """Generate a JS file containing inline WASM and glue JS file.

    Performs the following steps:
    1. Takes a cc_target and uses emscripten to compile it to WASM binary + glue JS.
    2. Takes the wasm_binary and inlines it to JS.
    2. The inlined wasm + glue JS is used to generate the final JS file.

    Example usage:
        cc_inline_wasm_udf_delta(
            name = "hello_udf_js",
            cc_target = ":hello",
        )

    Args:
        name: BUILD target name
        srcs: Names of the files that will be compiled to WASM
        tags: tags to propagate to rules
    """

    # Generate WASM + JS using emscripten
    inline_wasm_cc_binary(
        name = "{}_wasm_js_emscripten".format(name),
        srcs = srcs,
        outputs = [
            "{}_wasm_bin.wasm".format(name),
            "{}_glue.js".format(name),
        ],
        tags = tags,
    )

    inline_wasm_udf_js(
        name = name,
        wasm_binary = ":{}_wasm_bin.wasm".format(name),
        glue_js = ":{}_glue.js".format(name),
        tags = tags,
    )
