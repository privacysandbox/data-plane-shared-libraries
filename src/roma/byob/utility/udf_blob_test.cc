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

#include "src/roma/byob/utility/udf_blob.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <include/gmock/gmock-matchers.h>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

using ::privacy_sandbox::server_common::byob::UdfBlob;
using ::testing::StrEq;

namespace {
absl::StatusOr<std::string> GetFileContent(std::filesystem::path path) {
  const std::ifstream input_stream(path.c_str(), std::ios_base::binary);
  if (input_stream.fail()) {
    return absl::InternalError(
        absl::StrCat("Failed to open file ", path.c_str()));
  }
  std::stringstream buffer;
  buffer << input_stream.rdbuf();
  return buffer.str();
}

TEST(UdfBlob, CreatesFileWithEmptyBlob) {
  std::string content = "";
  auto udf_blob = UdfBlob::Create(content);
  ASSERT_TRUE(udf_blob.ok());

  auto content_from_file = GetFileContent((*udf_blob)());
  ASSERT_TRUE(content_from_file.ok()) << content_from_file.status();
  EXPECT_THAT(*content_from_file, StrEq(content));
}

TEST(UdfBlob, CreatesFileWithBlobContents) {
  std::string content = R"(|AzP`i)";
  auto udf_blob = UdfBlob::Create(content);
  ASSERT_TRUE(udf_blob.ok());

  auto content_from_file = GetFileContent((*udf_blob)());
  ASSERT_TRUE(content_from_file.ok()) << content_from_file.status();
  EXPECT_THAT(*content_from_file, StrEq(content));
}

TEST(UdfBlob, FileIsDeletedByDestructor) {
  std::filesystem::path filepath;

  {
    auto udf_blob = UdfBlob::Create("test blob");
    ASSERT_TRUE(udf_blob.ok());
    filepath = (*udf_blob)();

    // When udf_blob is in scope, the file must exist.
    EXPECT_TRUE(std::filesystem::exists(filepath));
  }

  // When udf_blob falls out-of-scope expect file to be deleted.
  EXPECT_FALSE(std::filesystem::exists(filepath));
}
}  // namespace
