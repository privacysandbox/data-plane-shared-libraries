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
#define DEBUG_GRPC_TAG_MANAGER
#include "core/network/src/grpc_tag_manager.h"

#include <gtest/gtest.h>

namespace google::scp::core {

// This increments a counter on destruction.
struct DestructorCounter {
  explicit DestructorCounter(size_t& ctr) : counter_(ctr) {}

  ~DestructorCounter() { counter_++; }

  size_t& counter_;
};

TEST(GrpcTagManagerTest, Alloc) {
  GrpcTagManager<int> mgr;
  auto* p = mgr.Allocate(10);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(*p, 10);
  EXPECT_EQ(mgr.Size(), 1UL);

  p = mgr.Allocate(30);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(*p, 30);
  EXPECT_EQ(mgr.Size(), 2UL);
}

TEST(GrpcTagManagerTest, Dealloc) {
  size_t counter = 0;
  GrpcTagManager<DestructorCounter> mgr;
  auto* p = mgr.Allocate(counter);
  ASSERT_NE(p, nullptr);
  EXPECT_EQ(mgr.Size(), 1UL);
  mgr.Deallocate(p);
  EXPECT_EQ(mgr.Size(), 0UL);
  EXPECT_EQ(counter, 1UL);
}

TEST(GrpcTagManagerTest, Destruct) {
  size_t counter = 0;
  {
    GrpcTagManager<DestructorCounter> mgr;
    for (int i = 0; i < 999; i++) {
      mgr.Allocate(counter);
    }
  }
  EXPECT_EQ(counter, 999UL);
}

}  // namespace google::scp::core
