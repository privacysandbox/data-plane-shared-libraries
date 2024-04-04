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

#include "src/core/common/concurrent_map/concurrent_map.h"

#include <gtest/gtest.h>

#include <atomic>
#include <thread>
#include <vector>

#include "src/core/common/uuid/uuid.h"
#include "src/core/test/scp_test_base.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::common::ConcurrentMap;
using google::scp::core::test::ResultIs;
using google::scp::core::test::ScpTestBase;

namespace google::scp::core::common::test {

class ConcurrentMapTests : public ScpTestBase {};

TEST_F(ConcurrentMapTests, InsertElement) {
  ConcurrentMap<int, int> map;

  int i;
  ASSERT_SUCCESS(map.Insert(std::make_pair(1, 1), i));
  EXPECT_EQ(i, 1);
}

TEST_F(ConcurrentMapTests, InsertExistingElement) {
  ConcurrentMap<int, int> map;

  int i;
  ASSERT_SUCCESS(map.Insert(std::make_pair(1, 1), i));
  ExecutionResult result = map.Insert(std::make_pair(1, 1), i);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_CONCURRENT_MAP_ENTRY_ALREADY_EXISTS)));
}

TEST_F(ConcurrentMapTests, DeleteExistingElement) {
  ConcurrentMap<int, int> map;

  int val = 1, key = 2;
  ASSERT_SUCCESS(map.Insert(std::make_pair(key, val), val));
  ASSERT_SUCCESS(map.Erase(key));
  EXPECT_THAT(map.Find(key, val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
}

TEST_F(ConcurrentMapTests, DeleteNonExistingElement) {
  ConcurrentMap<int, int> map;
  int i = 0;
  EXPECT_THAT(map.Erase(i),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONCURRENT_MAP_ENTRY_DOES_NOT_EXIST)));
}

TEST_F(ConcurrentMapTests, FindAnExistingElement) {
  ConcurrentMap<int, int> map;

  int i;
  int value;
  ASSERT_SUCCESS(map.Insert(std::make_pair(1, 1), i));
  ASSERT_SUCCESS(map.Find(i, value));
  EXPECT_EQ(value, 1);
}

TEST_F(ConcurrentMapTests, FindAnExistingElementUuid) {
  ConcurrentMap<Uuid, Uuid, UuidCompare> map;

  Uuid uuid_key = Uuid::GenerateUuid();
  Uuid uuid_value = Uuid::GenerateUuid();

  Uuid value;
  ASSERT_SUCCESS(map.Insert(std::make_pair(uuid_key, uuid_value), uuid_value));
  ASSERT_SUCCESS(map.Find(uuid_key, value));
  EXPECT_EQ(value, uuid_value);
}

TEST_F(ConcurrentMapTests, GetKeys) {
  ConcurrentMap<Uuid, Uuid, UuidCompare> map;

  Uuid uuid_key = Uuid::GenerateUuid();
  Uuid uuid_value = Uuid::GenerateUuid();

  Uuid uuid_key1 = Uuid::GenerateUuid();
  Uuid uuid_value1 = Uuid::GenerateUuid();

  ASSERT_SUCCESS(map.Insert(std::make_pair(uuid_key, uuid_value), uuid_value));
  ASSERT_SUCCESS(
      map.Insert(std::make_pair(uuid_key1, uuid_value1), uuid_value1));

  std::vector<Uuid> keys;
  ASSERT_SUCCESS(map.Keys(keys));

  if (keys[0] == uuid_key) {
    EXPECT_EQ(keys[1], uuid_key1);
  } else if (keys[0] == uuid_key1) {
    EXPECT_EQ(keys[1], uuid_key);
  } else {
    EXPECT_EQ(true, false);
  }
}
}  // namespace google::scp::core::common::test
