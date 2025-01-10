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

#include <fcntl.h>

#include <cstdlib>
#include <fstream>
#include <iostream>

#include "absl/status/status.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using google::protobuf::io::FileInputStream;
using google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using google::protobuf::util::SerializeDelimitedToFileDescriptor;
using privacy_sandbox::roma_byob::example::SampleRequest;
using privacy_sandbox::roma_byob::example::SampleResponse;

const std::filesystem::path kNewFileName = "new_file_path.txt";

SampleRequest ReadRequestFromFd(int fd) {
  SampleRequest req;
  FileInputStream stream(fd);
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &stream,
                                                           nullptr);
  return req;
}

void WriteResponseToFd(int fd, std::string_view greeting) {
  SampleResponse response;
  response.set_greeting(greeting);
  google::protobuf::util::SerializeDelimitedToFileDescriptor(response, fd);
}

bool FileExists(const std::filesystem::path& file_path) {
  return std::filesystem::exists(file_path);
}

// Parses the file system directory structure and tries to write a new file to
// it. If the file exists or if the file was able to be written fails. Else,
// succeeds.
absl::Status ParseFileSystemAndVerifyNoWrite(std::filesystem::path dir) {
  auto verify_no_write = [](const std::filesystem::path& path) {
    if (!std::filesystem::is_directory(path)) {
      return absl::OkStatus();
    }
    auto new_file_path = path / kNewFileName;
    if (std::filesystem::exists(new_file_path)) {
      return absl::AlreadyExistsError(absl::StrCat(
          "Failure. File ", new_file_path.c_str(), " already exists."));
    }
    if (int new_file_fd =
            ::open(new_file_path.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
        new_file_fd > 0) {
      return absl::InternalError(
          absl::StrCat("Failure. Able to write ", new_file_path.c_str(),
                       ". Expected read-only filesystem."));
    }
    return absl::OkStatus();
  };
  if (auto status = verify_no_write(dir); !status.ok()) {
    return status;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
    // Recursively traverse subdirectories
    if (auto status = verify_no_write(entry.path()); !status.ok()) {
      return status;
    }
  }
  return absl::OkStatus();
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!";
    return -1;
  }
  int fd = std::stoi(argv[1]);
  SampleRequest sample_request = ReadRequestFromFd(fd);
  auto status = ParseFileSystemAndVerifyNoWrite("/");
  if (status.ok()) {
    WriteResponseToFd(fd, "Success.");
  } else {
    WriteResponseToFd(fd, std::move(status).message());
  }
  return 0;
}
