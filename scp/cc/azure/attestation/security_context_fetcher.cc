/*
 * Portions Copyright (c) Microsoft Corporation
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

#include "security_context_fetcher.h"

std::vector<uint8_t> fetchSecurityContextFile(std::string file_path) {
  const char* dir = std::getenv("UVM_SECURITY_CONTEXT_DIR");
  if (!dir) {
    throw std::runtime_error(
        "UVM_SECURITY_CONTEXT_DIR environment variable is not set");
  }

  std::string full_path = std::string(dir) + file_path;
  std::ifstream file(full_path, std::ios::binary);

  if (!file) {
    throw std::runtime_error("Unable to open file at full_path: " + full_path);
  }

  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}