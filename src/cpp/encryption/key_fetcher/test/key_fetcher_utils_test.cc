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

#include "src/cpp/encryption/key_fetcher/src/key_fetcher_utils.h"

#include <string>

#include "include/gtest/gtest.h"

namespace privacy_sandbox::server_common {
namespace {

TEST(KeyFetcherUtilsTest, ToOhttpKeyIdSuccess) {
  std::string result = ToOhttpKeyId("3480000000000000");
  EXPECT_EQ(result, "52");
}

TEST(KeyFetcherUtilsTest, MaxOhttpKeyIdValue) {
  std::string result = ToOhttpKeyId("FF0000000");
  EXPECT_EQ(result, "255");
}

}  // namespace
}  // namespace privacy_sandbox::server_common
