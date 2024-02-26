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

#include "src/aws/proxy/freelist.h"

#include <gtest/gtest.h>

#include <thread>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "src/aws/proxy/buffer.h"

namespace google::scp::proxy {

namespace test {

using TestBuffer = BasicBuffer<64>;
using Block = TestBuffer::Block;

TEST(FreelistTest, MultiThread) {
  Freelist<Block> freelist;
  std::vector<Block*> blocks1;
  std::vector<Block*> blocks2;
  auto alloc = [&freelist](std::vector<Block*>& blocks) {
    blocks.reserve(100);
    for (int i = 0; i < 100; ++i) {
      blocks.push_back(freelist.New());
    }
  };
  std::thread t1([&]() { alloc(blocks1); });
  std::thread t2([&]() { alloc(blocks2); });
  t1.join();
  t2.join();

  EXPECT_EQ(freelist.Size(), 0);
  absl::flat_hash_set<void*> result_set;
  for (auto block : blocks1) {
    EXPECT_EQ(result_set.count(block), 0);
    result_set.insert(block);
  }
  for (Block* block : blocks2) {
    EXPECT_EQ(result_set.count(block), 0);
    result_set.insert(block);
  }
  EXPECT_EQ(result_set.size(), 200);

  auto dealloc = [&](std::vector<Block*>& blocks) {
    for (auto block : blocks) {
      freelist.Delete(block);
    }
  };
  std::thread t3([&]() { dealloc(blocks1); });
  std::thread t4([&]() { dealloc(blocks2); });
  t3.join();
  t4.join();
  EXPECT_EQ(freelist.Size(), 200);
}

TEST(FreelistTest, MultiThreadDeleteChain) {
  Freelist<Block> freelist;
  Block* head1 = nullptr;
  Block* head2 = nullptr;
  auto alloc = [&freelist](Block** chain_head) {
    *chain_head = nullptr;
    for (int i = 0; i < 100; ++i) {
      Block* new_block = freelist.New();
      new_block->next = *chain_head;
      *chain_head = new_block;
    }
  };
  std::thread t1([&]() { alloc(&head1); });
  std::thread t2([&]() { alloc(&head2); });
  t1.join();
  t2.join();

  EXPECT_EQ(freelist.Size(), 0);

  auto dealloc = [&](Block* head) { freelist.DeleteChain(head); };
  std::thread t3([&]() { dealloc(head1); });
  std::thread t4([&]() { dealloc(head2); });
  t3.join();
  t4.join();
  EXPECT_EQ(freelist.Size(), 200);
}

}  // namespace test
}  // namespace google::scp::proxy
