// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/gvisor/utility/utils.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include <include/gmock/gmock-matchers.h>

#include "absl/log/check.h"
#include "absl/status/statusor.h"

using ::testing::StrEq;

std::string GetFileContent(std::filesystem::path& path) {
  const std::ifstream input_stream(path.c_str(), std::ios_base::binary);
  CHECK(!input_stream.fail()) << "Failed to open file";
  std::stringstream buffer;
  buffer << input_stream.rdbuf();
  return buffer.str();
}

TEST(WriteBinaryBlobToFile, WritesEmptyBlob) {
  const std::string content = "";
  absl::StatusOr<std::filesystem::path> output_file =
      ::privacy_sandbox::server_common::gvisor::WriteBlobToFile(content);
  EXPECT_TRUE(output_file.status().ok());
  EXPECT_THAT(GetFileContent(*output_file), StrEq(content));
}

TEST(WriteBinaryBlobToFile, WritesBinaryBlob) {
  const std::string content = R"(FC)";

  absl::StatusOr<std::filesystem::path> output_file =
      ::privacy_sandbox::server_common::gvisor::WriteBlobToFile(content);

  EXPECT_TRUE(output_file.status().ok());
  EXPECT_THAT(GetFileContent(*output_file), StrEq(content));
}

TEST(DeleteFile, DeletesFile) {
  char tmp[] = "/tmp/test_XXXXXX";
  mktemp(tmp);
  CHECK_NE(tmp[0], '\0');
  std::filesystem::path filename = std::filesystem::path(tmp);
  std::ofstream ofs(filename);
  CHECK(!ofs.fail());
  ofs << "I am a test file!\n";
  ofs.close();

  EXPECT_TRUE(
      ::privacy_sandbox::server_common::gvisor::DeleteFile(filename).ok());
}
