/*
 * Copyright 2023 Google LLC
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

#include "core/common/lru_cache/src/lru_cache.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>

#include "absl/strings/str_cat.h"

using ::testing::SizeIs;
using ::testing::StrEq;

namespace google::scp::core::common::test {
TEST(LruCacheTest, CanAddAndGetElement) {
  LruCache<std::string, std::string> cache(10);
  auto key = "Some Key";
  auto value = "Some value";

  cache.Set(key, value);

  EXPECT_TRUE(cache.Contains(key));

  auto read_value = cache.Get(key);

  EXPECT_THAT(read_value, StrEq(value));
}

TEST(LruCacheTest, ClearShouldEmptyCache) {
  LruCache<std::string, std::string> cache(10);
  auto key = "Some Key";
  auto value = "Some value";

  cache.Set(key, value);
  EXPECT_TRUE(cache.Contains(key));

  EXPECT_EQ(cache.Size(), 1);

  cache.Clear();

  EXPECT_EQ(cache.Size(), 0);
}

TEST(LruCacheTest, CanAddAndGetMultipleElements) {
  LruCache<std::string, std::string> cache(10);

  for (int i = 0; i < 10; i++) {
    auto key = absl::StrCat("Some Key", i);
    auto value = absl::StrCat("Some value", i);

    cache.Set(key, value);

    EXPECT_TRUE(cache.Contains(key));

    auto read_value = cache.Get(key);

    EXPECT_THAT(read_value, StrEq(value));
  }
}

TEST(LruCacheTest, ShouldReplaceOldestItem) {
  constexpr size_t kCacheSize = 5;
  LruCache<std::string, std::string> cache(kCacheSize);

  // We add 5 items
  for (int i = 0; i < kCacheSize; i++) {
    auto key = absl::StrCat("Some Key", i);
    auto value = absl::StrCat("Some value", i);

    cache.Set(key, value);
    EXPECT_TRUE(cache.Contains(key));

    auto read_value = cache.Get(key);
    EXPECT_THAT(read_value, StrEq(value));
  }

  EXPECT_EQ(cache.Size(), kCacheSize);

  // We add a new element, and since the cache is at capacity, we should replace
  // the oldest element (the first one that was added)
  auto new_key = "New key";
  auto new_value = "New Value";
  cache.Set(new_key, new_value);
  EXPECT_TRUE(cache.Contains(new_key));

  auto read_value = cache.Get(new_key);
  EXPECT_THAT(read_value, StrEq(new_value));

  // Size should still be 5
  EXPECT_EQ(cache.Size(), 5);

  // The first element that was inserted shouldn't exist
  EXPECT_FALSE(cache.Contains("Some Key0"));
  // The rest should exist
  for (int i = 1; i < kCacheSize; i++) {
    auto key = absl::StrCat("Some Key", i);
    auto value = absl::StrCat("Some value", i);

    EXPECT_TRUE(cache.Contains(key));
    auto read_value = cache.Get(key);
    EXPECT_THAT(read_value, StrEq(value));
  }
}

TEST(LruCacheTest, LruPolicyShouldBeAffectedByGets) {
  LruCache<std::string, std::string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  // If we were to add an element as it is, then key1 should be evicted.
  // However, if we touch key1, then key2 should be evicted. Touch key1.
  EXPECT_THAT(cache.Get(key1), StrEq(value1));

  auto key3 = "Key3";
  auto value3 = "Value3";

  cache.Set(key3, value3);

  // Now key2 shouldn't be there
  EXPECT_FALSE(cache.Contains(key2));

  EXPECT_TRUE(cache.Contains(key1));
  EXPECT_TRUE(cache.Contains(key3));
}

TEST(LruCacheTest, LruPolicyShouldBeAffectedBySets) {
  LruCache<std::string, std::string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  // If we were to add an element as it is, then key1 should be evicted.
  // However, if we touch key1, then key2 should be evicted. Touch key1.
  auto new_value1 = "NewValue1";
  cache.Set(key1, new_value1);

  auto key3 = "Key3";
  auto value3 = "Value3";

  cache.Set(key3, value3);

  // Now key2 shouldn't be there
  EXPECT_FALSE(cache.Contains(key2));

  EXPECT_TRUE(cache.Contains(key1));
  EXPECT_TRUE(cache.Contains(key3));
}

TEST(LruCacheTest, ShouldBeAbleToReplaceValues) {
  LruCache<std::string, std::string> cache(2);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);
  EXPECT_THAT(cache.Get(key1), StrEq(value1));

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  auto new_value1 = "NewValue1";
  cache.Set(key1, new_value1);

  EXPECT_THAT(cache.Get(key1), StrEq(new_value1));
}

TEST(LruCacheTest, ShouldBeAbleToGetAllItems) {
  constexpr size_t kCacheSize = 2;
  LruCache<std::string, std::string> cache(kCacheSize);

  auto key1 = "Key1";
  auto value1 = "Value1";
  cache.Set(key1, value1);
  EXPECT_THAT(cache.Get(key1), StrEq(value1));

  auto key2 = "Key2";
  auto value2 = "Value2";
  cache.Set(key2, value2);

  auto all_items = cache.GetAll();

  EXPECT_THAT(all_items, SizeIs(kCacheSize));
  EXPECT_THAT(all_items[key1], StrEq(value1));
  EXPECT_THAT(all_items[key2], StrEq(value2));
}

TEST(LruCacheTest, AccessMiddleRegressionTest) {
  constexpr int kNumItems = 1000;
  for (int i = 0; i < kNumItems; ++i) {
    LruCache<int, int> cache(kNumItems);
    for (int j = 0; j < kNumItems; ++j) {
      cache.Set(j, j);
    }

    // Access middle before accessing the rest.
    EXPECT_EQ(cache.Get(i), i);
    for (int j = 0; j < kNumItems; ++j) {
      if (j != i) {
        EXPECT_EQ(cache.Get(j), j);
      }
    }
  }
}
}  // namespace google::scp::core::common::test
