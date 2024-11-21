
/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_ROMA_BYOB_UTILITY_UDF_BLOB_H_
#define SRC_ROMA_BYOB_UTILITY_UDF_BLOB_H_

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"

namespace privacy_sandbox::server_common::byob {
class UdfBlob final {
 public:
  // Factory method: Creates a file and populates it with 'blob'.
  static absl::StatusOr<UdfBlob> Create(std::string blob);

  explicit UdfBlob(std::filesystem::path file_path)
      : file_path_(std::move(file_path)) {}

  // Disable copy semantics.
  UdfBlob(const UdfBlob&) = delete;
  UdfBlob& operator=(const UdfBlob&) = delete;

  UdfBlob(UdfBlob&& other) noexcept
      : file_path_(std::exchange(other.file_path_, std::filesystem::path())) {}

  UdfBlob& operator=(UdfBlob&& other) noexcept {
    file_path_ = other.file_path_;
    return *this;
  }

  // Returns the path to the file with the blob.
  std::filesystem::path operator()() { return file_path_; }

  // Deletes the file to which the blob was written. Logs an error if delete
  // failed.
  ~UdfBlob();

 private:
  std::filesystem::path file_path_;
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_UTILITY_UDF_BLOB_H_
