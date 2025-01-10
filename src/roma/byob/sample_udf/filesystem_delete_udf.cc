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
#include <system_error>

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

// Parses the file system directory structure and tries to delete an existing
// file/directory. If a file/directory can be deleted to, it sends a fail
// message. Else, it succeeds.
absl::Status ParseFileSystemAndVerifyNoDelete(std::filesystem::path path) {
  auto verify_no_delete = [](const std::filesystem::path& path) {
    if (std::error_code ec; std::filesystem::remove_all(path, ec) !=
                            static_cast<std::uintmax_t>(-1)) {
      return absl::InternalError(
          absl::StrCat("Failure. Able to delete ", path.c_str(),
                       ". Expected read-only filesystem."));
    }
    return absl::OkStatus();
  };
  if (auto status = verify_no_delete(path); !status.ok()) {
    return status;
  }
  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(path)) {
    // Recursively traverse subdirectories
    if (auto status = verify_no_delete(entry.path()); !status.ok()) {
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
  auto status = ParseFileSystemAndVerifyNoDelete("/");
  if (status.ok()) {
    WriteResponseToFd(fd, "Success.");
  } else {
    WriteResponseToFd(fd, std::move(status).message());
  }
  return 0;
}
