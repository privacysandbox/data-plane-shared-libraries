# Copyright 2024 Google LLC
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

load("@rules_cc//cc:defs.bzl", "cc_binary")

def json_converter(*, name, src, out):
    cc_binary(
        name = "{}_converter".format(name),
        srcs = ["json_converter.cc"],
        deps = [
            "//src/roma/benchmark/serde:benchmark_service_cc_proto",
            "@nlohmann_json//:lib",
            "@com_google_absl//absl/strings",
            "@com_google_protobuf//:protobuf",
        ],
    )
    native.genrule(
        name = name,
        srcs = [
            src,
            ":{}_converter".format(name),
        ],
        outs = [out],
        cmd = "$(location {}_converter) $(location {}) $(location {})".format(name, src, out),
    )
