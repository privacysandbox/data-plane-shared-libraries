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

#include "src/roma/gvisor/utility/utils.h"

#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"

namespace privacy_sandbox::server_common::gvisor {

absl::StatusOr<std::filesystem::path> WriteBlobToFile(std::string blob) {
  char tmpfile[] = "/tmp/roma_gvisor_XXXXXX";
  char* filename = ::mktemp(tmpfile);
  if (filename == nullptr || *filename == '\0') {
    return absl::InternalError("Failed to create tempfile");
  }
  std::ofstream file_stream(std::string(filename),
                            std::ios::trunc | std::ios::binary);
  if (file_stream.fail()) {
    return absl::InternalError("Failed to open file stream for write");
  }
  file_stream << blob;
  file_stream.close();
  return std::filesystem::path(tmpfile);
}

absl::Status DeleteFile(std::filesystem::path& file_path) {
  if (!std::filesystem::exists(file_path)) {
    LOG(ERROR) << "File " << file_path.c_str() << " does not exist";
    return absl::OkStatus();
  }
  if (std::error_code ec; std::filesystem::remove_all(file_path, ec) < 0) {
    LOG(ERROR) << "Failed to delete " << file_path.c_str() << " with error "
               << ec.message() << std::endl;
    return absl::InternalError(absl::StrCat(
        "Failed to delete ", file_path.c_str(), " with error ", ec.message()));
  }
  return absl::OkStatus();
}
}  // namespace privacy_sandbox::server_common::gvisor
