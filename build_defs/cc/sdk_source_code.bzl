# Copyright 2022 Google LLC
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

load("//build_defs/cc:sdk_common.bzl", "sdk_common")
load("//build_defs/cc/aws:aws_sdk_cpp_source_code_deps.bzl", "import_aws_sdk_cpp")

def sdk_dependencies():
    sdk_common()

def sdk_dependencies2():
    import_aws_sdk_cpp()
