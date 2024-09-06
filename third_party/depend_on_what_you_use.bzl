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

"""
Creates a bazel aspect for depend_on_what_you_use for bazel dependencies and CPP includes.
"""

load("@depend_on_what_you_use//:defs.bzl", "dwyu_aspect_factory")

dwyu = dwyu_aspect_factory(
    skip_external_targets = True,
    # Must use json file for this librarys ignored_includes.
    # https://github.com/martis42/depend_on_what_you_use/blob/0.3.0/src/aspect/factory.bzl#L15-L17
    ignored_includes = Label("//third_party:depend_on_what_you_use_config"),
    # TODO - Turn on when ready to fix the codebase.
    # recursive = True,
)
