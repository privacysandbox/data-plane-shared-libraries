// Copyright 2023 Google LLC
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

#include "src/encryption/key_fetcher/key_fetcher_utils.h"

#include <gmock/gmock.h>

#include <string>

#include "include/gtest/gtest.h"

using ::testing::StrEq;

namespace privacy_sandbox::server_common {
namespace {

TEST(KeyFetcherUtilsTest, EmptyStr) {
  absl::StatusOr<std::string> result = ToOhttpKeyId("");
  ASSERT_FALSE(result.ok());
}

TEST(KeyFetcherUtilsTest, Short1Str) {
  absl::StatusOr<std::string> result = ToOhttpKeyId("1");
  ASSERT_FALSE(result.ok());
}

TEST(KeyFetcherUtilsTest, Short2Str) {
  absl::StatusOr<std::string> result = ToOhttpKeyId("12");
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, StrEq("18"));
}

TEST(KeyFetcherUtilsTest, ToOhttpKeyIdSuccess) {
  absl::StatusOr<std::string> result = ToOhttpKeyId("3480000000000000");
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, StrEq("52"));
}

TEST(KeyFetcherUtilsTest, MaxOhttpKeyIdValue) {
  absl::StatusOr<std::string> result = ToOhttpKeyId("FF0000000");
  ASSERT_TRUE(result.ok());
  EXPECT_THAT(*result, StrEq("255"));
}

}  // namespace
}  // namespace privacy_sandbox::server_common
