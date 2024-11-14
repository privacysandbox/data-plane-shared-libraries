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
#include "src/roma/byob/example/example.pb.h"

using ::privacy_sandbox::server_common::byob::example::EchoRequest;
using ::privacy_sandbox::server_common::byob::example::EchoResponse;

EchoRequest ReadRequestFromFd(int fd) {
  EchoRequest req;
  google::protobuf::io::FileInputStream stream(fd);
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &stream,
                                                           nullptr);
  return req;
}

void WriteResponseToFd(int fd, EchoResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Expecting exactly one argument" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  // Any initialization work can be done before this point.
  // The following line will result in a blocking read being performed by the
  // binary i.e. waiting for input before execution.
  // The EchoRequest proto is defined by the Trusted Server team. The UDF reads
  // request from the provided file descriptor.
  EchoRequest request = ReadRequestFromFd(fd);

  EchoResponse response;
  response.set_message(request.message());

  // Once the UDF is done executing, it should write the response (EchoResponse
  // in this case) to the provided file descriptor.
  WriteResponseToFd(fd, std::move(response));
  return 0;
}
