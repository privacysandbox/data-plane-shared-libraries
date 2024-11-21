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

#ifndef SRC_ROMA_BYOB_UTILITY_FILE_READER_H_
#define SRC_ROMA_BYOB_UTILITY_FILE_READER_H_

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/status/statusor.h"

namespace privacy_sandbox::server_common::byob {

/*
 * Manages the lifetime of the supplied file. Opens the file, mmap-s it contents
 * for read and deletes the file. The contents of the file are made available
 * till the calling object is in scope.
 */
class FileReader final {
 public:
  static absl::StatusOr<FileReader> Create(std::filesystem::path file_path);

  static absl::StatusOr<std::string_view> GetContent(
      const absl::StatusOr<FileReader>& file_reader);

  // Disable copy semantics.
  FileReader(const FileReader&) = delete;
  FileReader& operator=(const FileReader&) = delete;

  FileReader(FileReader&& other)
      : file_size_(std::exchange(other.file_size_, 0)),
        file_content_(std::exchange(other.file_content_, nullptr)) {}

  FileReader& operator=(FileReader&& other) noexcept {
    file_size_ = std::exchange(other.file_size_, 0);
    file_content_ = std::exchange(other.file_content_, nullptr);
    return *this;
  }

  // Deletes the file, munmap the mapped region if mmap was successful.
  ~FileReader();

  // Returns the contents of the blob if file exists and mmap succeeded. Else,
  // returns empty.
  std::string_view GetFileContent() const {
    return (file_size_ == 0) ? std::string_view()
                             : std::string_view(file_content_, file_size_);
  }

 private:
  explicit FileReader(char* file_content, uintmax_t file_size)
      : file_size_(file_size), file_content_(file_content) {}

  uintmax_t file_size_ = 0;
  char* file_content_ = nullptr;
};
}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_UTILITY_FILE_READER_H_
