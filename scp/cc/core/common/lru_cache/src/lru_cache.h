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

#ifndef CORE_COMMON_LRU_CACHE_SRC_LRU_CACHE_H_
#define CORE_COMMON_LRU_CACHE_SRC_LRU_CACHE_H_

#include <deque>
#include <mutex>
#include <tuple>
#include <utility>

#include "absl/container/flat_hash_map.h"

namespace google::scp::core::common {
/**
 * @brief Non-threadsafe least recently Used (LRU) cache
 *
 * @tparam TKey: a copyable type
 * @tparam TVal: a copyable type
 */
template <typename TKey, typename TVal>
class LruCache {
 public:
  explicit LruCache(size_t capacity) : capacity_(capacity) {}

  void Set(const TKey& key, TVal value) { Get(key) = std::move(value); }

  // Gets key from cache, inserting an entry if it doesn't exist.
  TVal& Get(const TKey& key) {
    if (const auto it = data_.find(key); it != data_.end()) {
      // If this is an existing element remove from freshness before reentry.
      freshness_.erase(it->second.freshness_it);
      freshness_.push_front(key);
      it->second.freshness_it = freshness_.begin();
      return it->second.cached_value;
    } else {
      if (data_.size() == capacity_) {
        // If the element is not in the cache and we reached capacity, let's
        // remove the LRU element. We look at the freshness to determine which
        // element is the LRU, and use the key from it to remove the data from
        // the data map.
        data_.erase(freshness_.back());
        freshness_.pop_back();
      }
      freshness_.push_front(key);

      // This second lookup is to insert a default value.
      ValueAndIterator& value = data_[key];
      value.freshness_it = freshness_.begin();
      return value.cached_value;
    }
  }

  size_t Size() const { return data_.size(); }

  size_t Capacity() const { return capacity_; }

  bool Contains(const TKey& key) const { return data_.contains(key); }

  void Clear() {
    data_.clear();
    freshness_.clear();
  }

  // Returns a map with copies of all key-value pairs in cache.
  absl::flat_hash_map<TKey, TVal> GetAll() const {
    absl::flat_hash_map<TKey, TVal> result;
    for (const auto& [key, value] : data_) {
      result[key] = value.cached_value;
    }
    return result;
  }

 private:
  const size_t capacity_;

  /**
   * @brief The order of access. Fresh items will be at the beginning, and stale
   * items at the end. The last item is what would be removed if the cache
   * reaches capacity.
   *
   */
  std::deque<TKey> freshness_;

  struct ValueAndIterator {
    // Value being cached.
    TVal cached_value;
    // Pointer to corresponding entry in freshness.
    typename decltype(freshness_)::iterator freshness_it;
  };

  absl::flat_hash_map<TKey, ValueAndIterator> data_;
};
}  // namespace google::scp::core::common

#endif  // CORE_COMMON_LRU_CACHE_SRC_LRU_CACHE_H_
