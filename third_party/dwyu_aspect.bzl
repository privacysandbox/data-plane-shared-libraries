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

""" Use depend_on_what_you_use for the codebase. """

load("@depend_on_what_you_use//:defs.bzl", "dwyu_aspect_factory")

dwyu_aspect = dwyu_aspect_factory(
    skip_external_targets = True,
    # TODO - Turn on when ready for the whole codebase.
    # recursive = True,
)
