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

#include <iostream>

#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::privacy_sandbox::roma_byob::example::LogRequest;
using ::privacy_sandbox::roma_byob::example::LogResponse;

int ReadRequestFromFd(int fd) {
  LogRequest log_req;
  FileInputStream input(fd);
  ParseDelimitedFromZeroCopyStream(&log_req, &input, nullptr);
  return log_req.log_count();
}

void WriteResponseToFd(int fd, LogResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::flush;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  int log_count = ReadRequestFromFd(fd);
  while (log_count--) {
    std::cerr << "I am benchmark stderr log.\n";
  }
  LogResponse log_response;
  WriteResponseToFd(fd, std::move(log_response));
  return 0;
}
