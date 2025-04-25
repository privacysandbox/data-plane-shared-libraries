// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef PARC_AZURE_SRC_SERVER_CPP_UTILS_BLOCKING_BOUNDED_QUEUE_H_
#define PARC_AZURE_SRC_SERVER_CPP_UTILS_BLOCKING_BOUNDED_QUEUE_H_

#include <condition_variable>
#include <deque>
#include <utility>

#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"

namespace privacysandbox::parc::utils {

template <typename T>
class BlockingBoundedQueue {
 public:
  explicit BlockingBoundedQueue(size_t max_size) : max_size_(max_size) {}

  // Pushes an item onto the queue. Blocks if the queue is full.
  void Put(T item) ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    // Wait while the queue is full
    // The predicate is checked before waiting and after waking up
    // to handle spurious wakeups.
    while (queue_.size() >= max_size_) {
      not_full_cv_.Wait(&mu_);
    }
    queue_.push_back(std::move(item));
    not_empty_cv_.Signal();
  }

  // Takes an item from the queue. Blocks if the queue is empty.
  T Take() ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    while (queue_.empty()) {
      not_empty_cv_.Wait(&mu_);
    }
    // Retrieve and remove the item
    T item = std::move(queue_.front());
    queue_.pop_front();
    // Signal one waiting producer (if any) that the queue is not full
    not_full_cv_.Signal();
    return item;
  }

  size_t Size() const ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return queue_.size();
  }

  bool IsEmpty() const ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    return queue_.empty();
  }

 private:
  size_t max_size_;
  std::deque<T> queue_ ABSL_GUARDED_BY(mu_);
  mutable absl::Mutex mu_;
  // Signaled when queue goes from empty to non-empty
  absl::CondVar not_empty_cv_;
  // Signaled when queue goes from full to non-full
  absl::CondVar not_full_cv_;
};

}  // namespace privacysandbox::parc::utils

#endif  // PARC_AZURE_SRC_SERVER_CPP_UTILS_BLOCKING_BOUNDED_QUEUE_H_
