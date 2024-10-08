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

#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::privacy_sandbox::roma_byob::example::SampleResponse;

void ReadRequestFromFd(int fd) {
  google::protobuf::Any bin_request;
  FileInputStream input(fd);
  ParseDelimitedFromZeroCopyStream(&bin_request, &input, nullptr);
}

void WriteResponseToFd(int fd, SampleResponse resp) {
  google::protobuf::Any any;
  any.PackFrom(std::move(resp));
  google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
}

int main(int argc, char* argv[]) {
  std::cout << "I am a stdout log." << std::endl;
  std::cerr << "I am a stderr log." << std::endl;
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  ReadRequestFromFd(fd);
  SampleResponse bin_response;
  bin_response.set_greeting("I am a UDF that logs.");
  WriteResponseToFd(fd, std::move(bin_response));
  return 0;
}
