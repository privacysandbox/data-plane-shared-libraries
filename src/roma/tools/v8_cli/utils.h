/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROMA_TOOLS_V8_CLI_UTILS_H_
#define ROMA_TOOLS_V8_CLI_UTILS_H_

#include <string>
#include <string_view>
#include <vector>

namespace google::scp::roma::tools::v8_cli {
constexpr std::string_view kFlagPrefix = "--";

constexpr std::string_view kTestDoublesLibraryPath =
    "src/roma/tools/v8_cli/test_doubles_library.js";

bool IsCustomFlag(const std::vector<std::string>& custom_flags,
                  std::string_view flag);

std::vector<std::string> ExtractAndSanitizeCustomFlags(
    int argc, char* argv[], const std::vector<std::string>& absl_flags);
}  // namespace google::scp::roma::tools::v8_cli

#endif  // ROMA_TOOLS_V8_CLI_UTILS_H_
