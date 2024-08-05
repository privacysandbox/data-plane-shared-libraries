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

#include "uuid_to_string.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/public/core/interface/execution_result.h"

#include "uuid.h"

using testing::StrEq;

namespace google::scp::core::common::test {

// Ensure that all implementations of Uuid ToString return identical values.
TEST(UuidToStringTests, UuidToString) {
  constexpr std::string_view uuid_str = "1794DACA-6CD8-0B88-E79E-8E4B730031C6";
  Uuid uuid;
  const ExecutionResult r = FromString(std::string(uuid_str), uuid);
  EXPECT_TRUE(bool(r));

  EXPECT_THAT(ToString(uuid), StrEq(uuid_str));
  EXPECT_THAT(ToStringAbslFormat(uuid), StrEq(uuid_str));
  EXPECT_THAT(ToStringAbslAppend(uuid), StrEq(uuid_str));
  EXPECT_THAT(ToStringFn(uuid, AppendHexLookupMap), StrEq(uuid_str));
  EXPECT_THAT(ToStringFn(uuid, AppendHexByte), StrEq(uuid_str));
}

}  // namespace google::scp::core::common::test
