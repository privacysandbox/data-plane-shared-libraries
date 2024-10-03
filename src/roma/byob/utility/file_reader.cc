// Copyright 2024 Google LLC
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

#include "src/roma/byob/utility/file_reader.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace privacy_sandbox::server_common::byob {
absl::StatusOr<FileReader> FileReader::Create(std::filesystem::path file_path) {
  if (!std::filesystem::exists(file_path)) {
    return absl::NotFoundError(
        absl::StrCat(file_path.c_str(), " does not exist."));
  } else if (!std::filesystem::is_regular_file(file_path)) {
    return absl::InvalidArgumentError(
        absl::StrCat(file_path.c_str(), "  is not a regular file."));
  }
  std::error_code error;
  uintmax_t file_size = std::filesystem::file_size(file_path, error);
  if (error) {
    return absl::InternalError(
        absl::StrCat("Failed to get file size: ", error.message()));
  }
  // If the file is empty, delete the file and return early.
  if (file_size == 0) {
    ::unlink(file_path.c_str());
    return FileReader(/*file_content=*/nullptr, 0);
  }
  int file_fd = ::open(file_path.c_str(), O_RDONLY);
  // Once the file is opened, unlink to mark it for deletion-on-close.
  ::unlink(file_path.c_str());
  if (file_fd < 0) {
    file_size = 0;
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to open ", file_path.c_str()));
  }
  void* mmap_file_ptr =
      ::mmap(/*addr=*/
             nullptr, file_size, PROT_READ, MAP_PRIVATE, file_fd,
             /*offset=*/0);
  // Closing the file_fd enables file deletion by the unlink call made in the
  // constructor. This is safe to do as the file contents have already been
  // mmap-ed for use.
  if (::close(file_fd) < 0) {
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to close ", file_path.c_str()));
  }
  if (mmap_file_ptr == reinterpret_cast<void*>(-1)) {
    file_size = 0;
    return absl::ErrnoToStatus(
        errno, absl::StrCat("Failed to mmap ", file_path.c_str()));
  }
  auto file_content = static_cast<char*>(mmap_file_ptr);
  return FileReader(file_content, file_size);
}

absl::StatusOr<std::string_view> FileReader::GetContent(
    const absl::StatusOr<FileReader>& file_reader) {
  if (!file_reader.ok()) {
    return file_reader.status();
  }
  return file_reader->GetFileContent();
}

FileReader::~FileReader() {
  // If file_size_ is 0, the file was not mmap-ed, nothing to clean up.
  if (file_size_ == 0) {
    return;
  }
  if (file_content_ == nullptr) {
    PLOG(ERROR) << "Expected file_content_ to be a non-null pointer";
    return;
  }
  if (::munmap(static_cast<void*>(file_content_), file_size_) != 0) {
    PLOG(ERROR) << "Failed to unmap " << file_content_ << ".";
  }
}
}  // namespace privacy_sandbox::server_common::byob
