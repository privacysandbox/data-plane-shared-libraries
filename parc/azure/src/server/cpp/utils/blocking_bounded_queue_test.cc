/*
 * Copyright 2025 Google LLC
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

#include "src/server/cpp/utils/blocking_bounded_queue.h"

#include <atomic>
#include <numeric>
#include <thread>
#include <vector>

#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

using privacysandbox::parc::utils::BlockingBoundedQueue;

// --- Basic Single-Threaded Tests ---

TEST(BlockingBoundedQueueTest, InitialState) {
  BlockingBoundedQueue<int> q(5);
  EXPECT_EQ(q.Size(), 0);
  EXPECT_TRUE(q.IsEmpty());
}

TEST(BlockingBoundedQueueTest, SimplePutTake) {
  BlockingBoundedQueue<int> q(5);
  q.Put(10);
  EXPECT_EQ(q.Size(), 1);
  EXPECT_FALSE(q.IsEmpty());
  EXPECT_EQ(q.Take(), 10);
  EXPECT_EQ(q.Size(), 0);
  EXPECT_TRUE(q.IsEmpty());
}

TEST(BlockingBoundedQueueTest, FifoOrder) {
  BlockingBoundedQueue<int> q(5);
  q.Put(1);
  q.Put(2);
  q.Put(3);
  EXPECT_EQ(q.Take(), 1);
  EXPECT_EQ(q.Take(), 2);
  EXPECT_EQ(q.Take(), 3);
}

// --- Concurrency Tests ---

TEST(BlockingBoundedQueueTest, TakeBlocksOnEmptyAndUnblocks) {
  BlockingBoundedQueue<int> q(5);
  absl::Notification take_started;
  absl::Notification item_taken;
  int taken_value = -1;

  std::thread consumer([&]() {
    take_started.Notify();   // Signal that Take is about to be called
    taken_value = q.Take();  // Should block here
    item_taken.Notify();
  });

  // Wait for consumer to be ready to Take (with timeout)
  ASSERT_TRUE(take_started.WaitForNotificationWithTimeout(absl::Seconds(1)));

  // Give the consumer a moment to potentially block (best effort)
  absl::SleepFor(absl::Milliseconds(50));
  EXPECT_FALSE(item_taken.HasBeenNotified());  // Should not have taken yet

  q.Put(123);  // Unblock the consumer

  // Wait for the consumer to actually take the item (with timeout)
  ASSERT_TRUE(item_taken.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(taken_value, 123);

  consumer.join();
  EXPECT_TRUE(q.IsEmpty());
}

TEST(BlockingBoundedQueueTest, PutBlocksOnFullAndUnblocks) {
  constexpr int capacity = 3;
  BlockingBoundedQueue<int> q(capacity);
  for (int i = 0; i < capacity; ++i) {
    q.Put(i);  // Fill the queue
  }
  EXPECT_EQ(q.Size(), capacity);

  absl::Notification put_started;
  absl::Notification item_put;

  std::thread producer([&]() {
    put_started.Notify();  // Signal that Put is about to be called
    q.Put(99);             // Should block here
    item_put.Notify();
  });

  // Wait for producer to be ready to Put (with timeout)
  ASSERT_TRUE(put_started.WaitForNotificationWithTimeout(absl::Seconds(1)));

  // Give the producer a moment to potentially block (best effort)
  absl::SleepFor(absl::Milliseconds(50));
  EXPECT_FALSE(item_put.HasBeenNotified());  // Should not have put yet
  EXPECT_EQ(q.Size(), capacity);             // Still full

  EXPECT_EQ(q.Take(), 0);  // Make space

  // Wait for the producer to actually put the item (with timeout)
  ASSERT_TRUE(item_put.WaitForNotificationWithTimeout(absl::Seconds(1)));
  EXPECT_EQ(q.Size(), capacity);  // Should be full again

  producer.join();

  // Verify the final item is there
  EXPECT_EQ(q.Take(), 1);
  EXPECT_EQ(q.Take(), 2);
  EXPECT_EQ(q.Take(), 99);
  EXPECT_TRUE(q.IsEmpty());
}
