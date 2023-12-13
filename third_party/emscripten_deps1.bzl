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

load("@emsdk//:deps.bzl", emsdk_deps = "deps")

EMSCRIPTEN_LINKOPTS = [
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

def emscripten_deps1():
    emsdk_deps()
