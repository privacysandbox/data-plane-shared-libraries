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

#include <condition_variable>
#include <mutex>

namespace google::scp::core {

// Essentially a mutex but is allowed to be unlocked by the non-owning thread.
class BinarySemaphore {
 public:
  // Releases the semaphore for someone else to own it.
  void Release() {
    std::lock_guard<decltype(mutex_)> lock(mutex_);
    owned_ = false;
    condition_.notify_one();
  }

  // Blocks until the semaphore can be acquired.
  void Acquire() {
    std::unique_lock<decltype(mutex_)> lock(mutex_);
    while (owned_)  // Handle spurious wake-ups.
      condition_.wait(lock);
    owned_ = true;
  }

 private:
  std::mutex mutex_;
  std::condition_variable condition_;
  bool owned_{false};  // Initialized as unlocked.
};

}  // namespace google::scp::core
