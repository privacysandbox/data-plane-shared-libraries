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

#include "src/roma/byob/utility/file_reader.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <sys/stat.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "absl/log/check.h"
#include "absl/strings/str_cat.h"
namespace {

using ::privacy_sandbox::server_common::byob::FileReader;

std::filesystem::path CreateFile(std::string_view content) {
  std::filesystem::path filename(absl::StrCat(std::tmpnam(nullptr), ".txt"));
  CHECK(!std::filesystem::exists(filename));
  std::ofstream file(filename.c_str());
  CHECK(file.is_open());
  file << content;
  file.close();
  CHECK(std::filesystem::exists(filename));
  return filename;
}

TEST(FileReaderTest, FileDoesNotExistGetFileContentsReturnsEmpty) {
  auto file_reader = FileReader::Create(std::filesystem::path("/tmp/foo.txt"));

  EXPECT_FALSE(file_reader.ok()) << file_reader.status();
}

TEST(FileReaderTest, DirAsInputGetFileContentsReturnsEmpty) {
  std::filesystem::path dir = std::filesystem::path("/tmp");
  CHECK(std::filesystem::is_directory(dir));
  auto file_reader = FileReader::Create(dir);

  EXPECT_FALSE(file_reader.ok()) << file_reader.status();
}

TEST(FileReaderTest, EmptyFileGetFileContentsReturnsEmpty) {
  std::filesystem::path file = CreateFile("");

  auto file_reader = FileReader::Create(file);

  EXPECT_TRUE(file_reader.ok()) << file_reader.status();
  EXPECT_THAT(file_reader->GetFileContent(), ::testing::IsEmpty());
}

TEST(FileReaderTest, FileDeletedAfterConstructorCall) {
  std::string content;
  std::filesystem::path file = CreateFile(content);

  auto file_reader = FileReader::Create(file);

  EXPECT_FALSE(std::filesystem::exists(file));
}

TEST(FileReaderTest, GetFileContentReturnsContent) {
  std::string content = "eieowieowioeiowieow";
  std::filesystem::path file = CreateFile(content);

  auto file_reader = FileReader::Create(file);

  EXPECT_TRUE(file_reader.ok()) << file_reader.status();
  EXPECT_THAT(file_reader->GetFileContent(), ::testing::StrEq(content));
}

// TODO: ashruti - Modify this test to run as nonroot for everything to WAI.
TEST(FileReaderTest, DISABLED_GetFileContentReturnsEmptyForNoFilePerms) {
  std::string content = "bananas";
  std::filesystem::path file = CreateFile(content);
  std::error_code ec;
  std::filesystem::permissions(file, std::filesystem::perms::others_exec,
                               std::filesystem::perm_options::replace, ec);
  CHECK(!ec) << ec.message();

  auto file_reader = FileReader::Create(file);

  EXPECT_FALSE(file_reader.ok()) << file_reader.status();
}

TEST(FileReaderTest, GetFileContentReturnsContentForSpecialSymbols) {
  std::string content =
      "é, à, ö, ñ,  😀, 🌍, 🎉, 👋, etc.漢 (Chinese), こんにちは (Japanese), "
      "به متنی(Persian), etc. ©, ®, €, £, µ, ¥, etc.";
  std::filesystem::path file = CreateFile(content);

  auto file_reader = FileReader::Create(file);

  EXPECT_TRUE(file_reader.ok()) << file_reader.status();
  EXPECT_THAT(file_reader->GetFileContent(), ::testing::StrEq(content));
}
}  // namespace
