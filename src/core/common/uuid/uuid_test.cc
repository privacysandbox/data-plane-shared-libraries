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

#include "src/core/common/uuid/uuid.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "src/core/common/uuid/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::test::ResultIs;
using testing::StrEq;

namespace google::scp::core::common::test {

TEST(UuidTests, UuidGeneration) {
  Uuid uuid = Uuid::GenerateUuid();

  EXPECT_NE(uuid.high, 0);
  EXPECT_NE(uuid.low, 0);
}

TEST(UuidTests, UuidToString) {
  constexpr std::string_view uuid_str = "1794CADF-6CD8-0B88-E79E-8E4B730042C6";
  Uuid uuid;
  FromString(std::string(uuid_str), uuid);

  const auto uuid_string = ToString(uuid);
  EXPECT_THAT(uuid_string, StrEq(uuid_str));

  Uuid parsed_uuid;
  ASSERT_SUCCESS(FromString(uuid_string, parsed_uuid));
  EXPECT_EQ(parsed_uuid, uuid);
}

TEST(UuidTests, InvalidUuidString) {
  Uuid parsed_uuid;
  EXPECT_THAT(
      FromString("123", parsed_uuid),
      ResultIs(FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING)));

  // dashes expected in positions 8, 13, 18 and 23
  EXPECT_THAT(
      FromString("3E2A3D09-48ED-A355rD346-AD7DC6CB0909", parsed_uuid),
      ResultIs(FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING)));

  // invalid hex value 'R'
  EXPECT_THAT(
      FromString("3E2A3D09-48RD-A355-D346-AD7DC6CB0909", parsed_uuid),
      ResultIs(FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING)));

  // lowercase hex values
  EXPECT_THAT(
      FromString("3e2a3d09-48ed-a355-d346-ad7dc6cb0909", parsed_uuid),
      ResultIs(FailureExecutionResult(core::errors::SC_UUID_INVALID_STRING)));
}

}  // namespace google::scp::core::common::test
