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

#ifndef ROMA_METADATA_STORAGE_METADATA_STORAGE_H_
#define ROMA_METADATA_STORAGE_METADATA_STORAGE_H_

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "src/roma/metadata_storage/thread_safe_map.h"

namespace google::scp::roma::metadata_storage {

// MetadataStorage acts as a thin wrapper around ThreadSafeMap, providing a
// simplified interface for metadata operations.
template <typename TMetadata>
class MetadataStorage {
 public:
  absl::Status Add(std::string key, TMetadata val) {
    return metadata_storage_.Add(std::move(key), std::move(val));
  }

  absl::Status Delete(std::string_view key) {
    return metadata_storage_.Delete(key);
  }

  ThreadSafeMap<TMetadata>& GetMetadataMap() { return metadata_storage_; }

 private:
  ThreadSafeMap<TMetadata> metadata_storage_;
};

}  // namespace google::scp::roma::metadata_storage

#endif  // ROMA_METADATA_STORAGE_METADATA_STORAGE_H_
