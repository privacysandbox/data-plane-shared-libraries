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

#ifndef ROMA_METADATA_STORAGE_THREAD_SAFE_MAP_H_
#define ROMA_METADATA_STORAGE_THREAD_SAFE_MAP_H_

#include <string>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/node_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "src/util/status_macro/status_macros.h"

namespace google::scp::roma::metadata_storage {

// Supports thread safe insertion, deletion, and lookups. Provides client with
// row-level lock associated for a value to allow for thread-safe reading of
// values. Lock row-level lock before reading value and unlock after.
template <typename V>
class ThreadSafeMap {
 public:
  absl::Status Add(std::string key, V val)
      ABSL_LOCKS_EXCLUDED(mutex_map_mutex_) {
    absl::MutexLock mu_lock(&mutex_map_mutex_);
    absl::MutexLock lock(&key_value_map_mutex_);
    auto [it, inserted] =
        key_value_map_.try_emplace(std::move(key), std::move(val));
    if (!inserted) {
      return absl::AlreadyExistsError(
          "UUID for Invocation Request already added to map");
    }
    mutex_map_.try_emplace(it->first);  // create a mutex for key
    return absl::OkStatus();
  }

  absl::Status Delete(std::string_view key)
      ABSL_LOCKS_EXCLUDED(mutex_map_mutex_) {
    absl::MutexLock mu_lock(&mutex_map_mutex_);
    auto mutex_it = mutex_map_.find(key);
    if (mutex_it == mutex_map_.end()) {
      return absl::NotFoundError(
          "Mutex associated with metadata could not be found");
    }
    {
      absl::MutexLock row_lock(&mutex_it->second);
      absl::MutexLock lock(&key_value_map_mutex_);

      if (auto it = key_value_map_.find(key); it != key_value_map_.end()) {
        key_value_map_.erase(it);
      } else {
        return absl::NotFoundError("Metadata could not be found");
      }
    }
    mutex_map_.erase(mutex_it);
    return absl::OkStatus();
  }

  absl::StatusOr<V*> GetValue(std::string_view key)
      ABSL_LOCKS_EXCLUDED(key_value_map_mutex_) {
    absl::MutexLock lock(&key_value_map_mutex_);
    if (const auto it = key_value_map_.find(key); it != key_value_map_.end()) {
      return &(it->second);
    }
    return absl::NotFoundError(
        absl::StrCat("Value with key: ", key, " could not be found"));
  }

  absl::StatusOr<absl::Mutex*> GetMutex(std::string_view key)
      ABSL_LOCKS_EXCLUDED(mutex_map_mutex_) {
    absl::MutexLock lock(&mutex_map_mutex_);
    if (const auto it = mutex_map_.find(key); it != mutex_map_.end()) {
      return &(it->second);
    }
    return absl::NotFoundError(
        absl::StrCat("Mutex with key: ", key, " could not be found"));
  }

 private:
  absl::node_hash_map<std::string, V> key_value_map_
      ABSL_GUARDED_BY(key_value_map_mutex_);
  absl::node_hash_map<std::string_view, absl::Mutex> mutex_map_
      ABSL_GUARDED_BY(mutex_map_mutex_);
  absl::Mutex key_value_map_mutex_;
  absl::Mutex mutex_map_mutex_;
};

template <typename V>
class ScopedValueReader {
 public:
  static absl::StatusOr<ScopedValueReader<V>> Create(ThreadSafeMap<V>& map,
                                                     std::string_view key) {
    PS_ASSIGN_OR_RETURN(auto mutex, map.GetMutex(key));
    return ScopedValueReader(map, key, mutex);
  }

  absl::StatusOr<V*> Get() {
    mutex_->AssertReaderHeld();
    return map_.GetValue(key_);
  }

  // Disable copy semantics.
  ScopedValueReader(const ScopedValueReader&) = delete;

  ScopedValueReader& operator=(const ScopedValueReader&) = delete;

  ScopedValueReader(ScopedValueReader&& other)
      : map_(other.map_), key_(other.key_), mutex_(other.mutex_) {
    other.mutex_ = nullptr;
  }

  ScopedValueReader& operator=(ScopedValueReader&& other) {
    map_ = other.map_;
    key_ = other.key_;
    mutex_ = other.mutex_;
    other.mutex_ = nullptr;
  }

  ~ScopedValueReader() {
    if (mutex_) {
      mutex_->Unlock();
    }
  }

 private:
  ScopedValueReader(ThreadSafeMap<V>& map, std::string_view key,
                    absl::Mutex* mutex)
      : map_(map), key_(key), mutex_(mutex) {
    mutex_->Lock();
  }

  ThreadSafeMap<V>& map_;
  std::string_view key_;
  absl::Mutex* mutex_;
};

}  // namespace google::scp::roma::metadata_storage

#endif  // ROMA_METADATA_STORAGE_THREAD_SAFE_MAP_H_
