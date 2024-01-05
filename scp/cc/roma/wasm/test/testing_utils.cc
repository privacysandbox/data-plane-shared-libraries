/*
 * Copyright 2022 Google LLC
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
#include "testing_utils.h"

#include <fstream>

#include "absl/log/check.h"

namespace google::scp::roma::wasm::testing {
std::vector<char> WasmTestingUtils::LoadWasmFile(std::string_view file_path) {
  std::ifstream input_file(std::string{file_path});
  CHECK(input_file.good()) << "File: " << file_path << " does not exist.";
  input_file.seekg(0, std::ios::end);
  const size_t filesize = input_file.tellg();
  input_file.seekg(0, std::ios::beg);
  std::vector<char> file_contents;
  file_contents.resize(filesize / sizeof(uint8_t));
  input_file.read(file_contents.data(), filesize);
  return file_contents;
}

std::string WasmTestingUtils::LoadJsWithWasmFile(std::string_view file_path) {
  std::ifstream input_file(std::string{file_path});
  CHECK(input_file.good()) << "File: " << file_path << " does not exist.";
  std::stringstream buffer;
  buffer << input_file.rdbuf();
  return buffer.str();
}
}  // namespace google::scp::roma::wasm::testing
