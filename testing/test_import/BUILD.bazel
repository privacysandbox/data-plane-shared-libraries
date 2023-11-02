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

"""targets to validate the import of workspace dependencies."""

load("@rules_proto//proto:defs.bzl", "proto_descriptor_set")

alias(
    name = "aws_sqs",
    actual = "@aws_sdk_cpp//:sqs",
)

alias(
    name = "protobuf",
    actual = "@com_google_protobuf//:protobuf",
)

alias(
    name = "grpc_cli",
    actual = "@com_github_grpc_grpc//test/cpp/util:grpc_cli",
)

proto_descriptor_set(
    name = "helloworld_descriptor_set",
    visibility = ["//visibility:public"],
    deps = ["@com_github_grpc_grpc//examples/protos:helloworld_proto"],
)
