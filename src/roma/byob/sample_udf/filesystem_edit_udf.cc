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
#include <filesystem>
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

// Parses the file system directory structure and tries to edit an existing
// file. If a file can be written to, it sends a fail message. Else, it
// succeeds.
absl::Status ParseFileSystemAndVerifyNoEdit(std::filesystem::path dir) {
  auto verify_no_edit = [](const std::filesystem::path& path) {
    if (!std::filesystem::is_regular_file(path)) {
      return absl::OkStatus();
    }
    std::ofstream out_file(path.c_str());
    if (out_file.fail()) {
      return absl::OkStatus();
    }
    out_file << "Able to edit";
    out_file.close();
    return absl::InternalError(
        absl::StrCat("Failure. Able to edit ", path.c_str(),
                     ". Expected read-only filesystem."));
  };
  if (auto status = verify_no_edit(dir); !status.ok()) {
    return status;
  }
  for (const auto& entry : std::filesystem::recursive_directory_iterator(dir)) {
    // Recursively traverse subdirectories
    if (auto status = verify_no_edit(entry.path()); !status.ok()) {
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
  auto status = ParseFileSystemAndVerifyNoEdit("/");
  if (status.ok()) {
    WriteResponseToFd(fd, "Success.");
  } else {
    WriteResponseToFd(fd, std::move(status).message());
  }
  return 0;
}
