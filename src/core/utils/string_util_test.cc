// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/core/utils/string_util.h"

#include <gtest/gtest.h>

#include <list>
#include <string>

namespace google::scp::core::utils::test {
TEST(StringUtilTest, ShouldHandleEmptyString) {
  std::string str = "";
  std::string delimiter = ",";
  std::list<std::string> output;
  SplitStringByDelimiter(str, delimiter, output);

  EXPECT_EQ(output, std::list<std::string>({""}));
}

TEST(StringUtilTest, ShouldHandleSingleItem) {
  std::string str = "a";
  std::string delimiter = ",";
  std::list<std::string> output;
  SplitStringByDelimiter(str, delimiter, output);

  EXPECT_EQ(output, std::list<std::string>({"a"}));
}

TEST(StringUtilTest, ShouldHandleSingleCharDelimiter) {
  std::string str = "a,b,c";
  std::string delimiter = ",";
  std::list<std::string> output;
  SplitStringByDelimiter(str, delimiter, output);

  EXPECT_EQ(output, std::list<std::string>({"a", "b", "c"}));
}

TEST(StringUtilTest, ShouldHandleMultiCharDelimiter) {
  std::string str = "a--b--c";
  std::string delimiter = "--";
  std::list<std::string> output;
  SplitStringByDelimiter(str, delimiter, output);

  EXPECT_EQ(output, std::list<std::string>({"a", "b", "c"}));
}
}  // namespace google::scp::core::utils::test
