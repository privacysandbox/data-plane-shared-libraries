/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_UTIL_FILE_FILE_UTIL_H_
#define SRC_UTIL_FILE_FILE_UTIL_H_

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace privacy_sandbox::server_common {

constexpr std::string_view kPathFailed = "Failed to load file from path: ";

// Gets the contents of the provided path.
absl::StatusOr<std::string> GetFileContent(absl::string_view path,
                                           bool log_on_error = false);

// Writes the provided contents to the file path.
absl::Status WriteToFile(absl::string_view path, absl::string_view contents,
                         bool log_on_error = false);

}  // namespace privacy_sandbox::server_common

#endif  // SRC_UTIL_FILE_FILE_UTIL_H_
