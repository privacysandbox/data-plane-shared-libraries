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
    base64_js = """function base64ToUint8Array(base64) {
  const BASE64_CHARS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';

  // Remove non-base64 characters
  const cleanInput = base64.replace(/[^A-Za-z0-9+/=]/g, '');

  // Get padding length (number of '=' at the end)
  const paddingLength = cleanInput.match(/=*$$/)[0].length;
  const encodedLength = cleanInput.length;
  const decodedLength = Math.floor((encodedLength * 3) / 4) - paddingLength;

  const bytes = new Uint8Array(decodedLength);
  const padding = 64;

  let decoded = 0;

  for (let i = 0; i < encodedLength; i += 4) {
    // Get 4 characters from the input, then find their positions in the BASE64_CHARS string
    const enc1 = BASE64_CHARS.indexOf(cleanInput.charAt(i));
    const enc2 = BASE64_CHARS.indexOf(cleanInput.charAt(i + 1));
    const enc3 = i + 2 < encodedLength ? BASE64_CHARS.indexOf(cleanInput.charAt(i + 2)) : padding;
    const enc4 = i + 3 < encodedLength ? BASE64_CHARS.indexOf(cleanInput.charAt(i + 3)) : padding;

    // Calculate byte values from index positions
    const triplet = (enc1 << 18) | (enc2 << 12) | ((enc3 & 63) << 6) | (enc4 & 63);

    if (decoded < decodedLength) bytes[decoded++] = (triplet >> 16) & 0xFF;
    if (decoded < decodedLength) bytes[decoded++] = (triplet >> 8) & 0xFF;
    if (decoded < decodedLength) bytes[decoded++] = triplet & 0xFF;
  }

  return bytes;
}"""
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
        cmd_bash = """WASM_BASE64=$$(base64 -w0 $(location {wasm_binary}))
cat << EOF > $@
let wasm_base64 = "$$WASM_BASE64"

{base64_js}

let wasm_array = base64ToUint8Array(wasm_base64);
$$(cat $(location {glue_js}))
{module_js}
EOF
""".format(
            wasm_binary = wasm_binary,
            glue_js = glue_js,
            module_js = get_module_js,
            base64_js = base64_js,
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
