/*
 * Copyright 2024 Google LLC
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

#include "src/roma/metadata_storage/thread_safe_map.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string>
#include <thread>
#include <vector>

#include "absl/strings/str_cat.h"

namespace google::scp::roma::metadata_storage::test {

TEST(ThreadSafeMapTest, AddAndGetSingleValue) {
  ThreadSafeMap<std::string> map;
  EXPECT_TRUE(map.Add("key1", "val1").ok());

  auto value_or_status = map.GetValue("key1");
  EXPECT_TRUE(value_or_status.ok());
  EXPECT_EQ(*value_or_status.value(), "val1");
}

TEST(ThreadSafeMapTest, AddDuplicateKey) {
  ThreadSafeMap<std::string> map;
  EXPECT_TRUE(map.Add("key1", "val1").ok());
  EXPECT_FALSE(map.Add("key1", "val2").ok());
}

TEST(ThreadSafeMapTest, DeleteExistingKey) {
  ThreadSafeMap<std::string> map;
  EXPECT_TRUE(map.Add("key1", "val1").ok());
  EXPECT_TRUE(map.Delete("key1").ok());
  EXPECT_FALSE(map.Delete("key1").ok());
}

TEST(ThreadSafeMapTest, GetValueNotFound) {
  ThreadSafeMap<std::string> map;
  EXPECT_FALSE(map.GetValue("key1").ok());
}

TEST(ThreadSafeMapTest, GetMutexNotFound) {
  ThreadSafeMap<std::string> map;
  EXPECT_FALSE(map.GetMutex("key1").ok());
}

TEST(ThreadSafeMapTest, ConcurrentAddAndGet) {
  ThreadSafeMap<int> map;
  constexpr int num_threads = 10;
  constexpr int iterations = 1000;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&map, i] {
      for (int j = 0; j < iterations; ++j) {
        EXPECT_TRUE(map.Add(absl::StrCat("key_", i * iterations + j),
                            i * iterations + j)
                        .ok());
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
  threads.clear();

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&map, i] {
      for (int j = 0; j < iterations; ++j) {
        auto key = absl::StrCat("key_", i * iterations + j);
        auto reader = ScopedValueReader<int>::Create(map, key);
        ASSERT_TRUE(reader.ok());
        auto value = reader->Get();
        EXPECT_TRUE(value.ok());
        EXPECT_EQ(**value, i * iterations + j);
      }
    });
  }
  for (auto& thread : threads) {
    thread.join();
  }
}

TEST(ThreadSafeMapTest, ConcurrentAddAndDelete) {
  ThreadSafeMap<int> map;
  constexpr int num_threads = 10;
  constexpr int iterations = 10;

  std::vector<std::thread> threads;
  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&map, i] {
      for (int j = 0; j < iterations; ++j) {
        EXPECT_TRUE(map.Add(absl::StrCat("key_", i * iterations + j),
                            i * iterations + j)
                        .ok());
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
  threads.clear();

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&map, i] {
      std::vector<std::thread> row_level_threads;
      for (int j = 0; j < iterations; ++j) {
        row_level_threads.emplace_back([&map, i, j] {
          EXPECT_TRUE(
              map.Delete(absl::StrCat("key_", i * iterations + j)).ok());
        });
      }
      for (auto& thread : row_level_threads) {
        thread.join();
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}

}  // namespace google::scp::roma::metadata_storage::test
