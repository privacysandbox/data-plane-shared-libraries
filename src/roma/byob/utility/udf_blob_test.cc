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

using ::privacy_sandbox::server_common::byob::UdfBlob;
using ::testing::StrEq;

namespace {
std::string GetFileContent(std::filesystem::path path) {
  const std::ifstream input_stream(path.c_str(), std::ios_base::binary);
  CHECK(!input_stream.fail()) << "Failed to open file";
  std::stringstream buffer;
  buffer << input_stream.rdbuf();
  return buffer.str();
}

TEST(UdfBlob, CreatesFileWithEmptyBlob) {
  std::string content = "";
  auto udf_blob = UdfBlob::Create(content);
  CHECK_OK(udf_blob);

  EXPECT_THAT(GetFileContent((*udf_blob)()), StrEq(content));
}

TEST(UdfBlob, CreatesFileWithBlobContents) {
  std::string content = R"(|AzP`i)";
  auto udf_blob = UdfBlob::Create(content);
  CHECK_OK(udf_blob);

  EXPECT_THAT(GetFileContent((*udf_blob)()), StrEq(content));
}

TEST(UdfBlob, FileIsDeletedByDestructor) {
  std::filesystem::path filepath;

  {
    auto udf_blob = UdfBlob::Create("test blob");
    CHECK_OK(udf_blob);
    filepath = (*udf_blob)();

    // When udf_blob is in scope, expect file exit.
    EXPECT_TRUE(std::filesystem::exists(filepath));
  }

  // When udf_blob falls out-of-scope expect file to be deleted.
  EXPECT_FALSE(std::filesystem::exists(filepath));
}
}  // namespace
