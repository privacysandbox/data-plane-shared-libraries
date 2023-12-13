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

load("@emsdk//:emscripten_deps.bzl", emsdk_emscripten_deps = "emscripten_deps")
load("@emsdk//:toolchains.bzl", "register_emscripten_toolchains")
load("@google_privacysandbox_servers_common//third_party:cpp_deps.bzl", "EMSCRIPTEN_VER")

def emscripten_deps2():
    emsdk_emscripten_deps(emscripten_version = EMSCRIPTEN_VER)
    register_emscripten_toolchains()
