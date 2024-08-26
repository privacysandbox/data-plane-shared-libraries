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

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/example/example.pb.h"

using ::privacy_sandbox::server_common::byob::example::EchoRequest;
using ::privacy_sandbox::server_common::byob::example::EchoResponse;

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 2) {
    LOG(ERROR) << "Not enough arguments!";
    return -1;
  }
  int32_t fd;
  CHECK(absl::SimpleAtoi(argv[1], &fd))
      << "Conversion of write file descriptor string to int failed";
  EchoRequest request;
  // Any initialization work can be done before this point.
  // The following line will result in a blocking read being performed by the
  // binary i.e. waiting for input before execution.
  // The EchoRequest proto is defined by the Trusted Server team. The UDF reads
  // request from the provided file descriptor.
  {
    ::google::protobuf::Any any;
    ::google::protobuf::io::FileInputStream input(fd);
    ::google::protobuf::util::ParseDelimitedFromZeroCopyStream(&any, &input,
                                                               nullptr);
    any.UnpackTo(&request);
  }

  EchoResponse response;
  response.set_message(request.message());

  // Once the UDF is done executing, it should write the response (EchoResponse
  // in this case) to the provided file descriptor.
  ::google::protobuf::Any any;
  any.PackFrom(response);
  ::google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
  return 0;
}
