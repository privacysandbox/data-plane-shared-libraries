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

#include <stdlib.h>
#include <unistd.h>

#include <iostream>

#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::roma_byob::example::SampleRequest;

void ReadRequestFromFd(int fd) {
  SampleRequest bin_request;
  FileInputStream input(fd);
  ParseDelimitedFromZeroCopyStream(&bin_request, &input, nullptr);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!";
    return -1;
  }
  int fd = std::stoi(argv[1]);
  ReadRequestFromFd(fd);
  ::pause();
}
