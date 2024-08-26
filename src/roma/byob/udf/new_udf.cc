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
#include "src/roma/byob/udf/sample.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::privacy_sandbox::server_common::byob::SampleResponse;

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 2) {
    LOG(ERROR) << "Not enough arguments!";
    return -1;
  }
  int32_t fd;
  CHECK(absl::SimpleAtoi(argv[1], &fd))
      << "Conversion of file descriptor string to int failed";
  {
    google::protobuf::Any bin_request;
    FileInputStream input(fd);
    ParseDelimitedFromZeroCopyStream(&bin_request, &input, nullptr);
  }
  SampleResponse bin_response;
  bin_response.set_greeting("I am a new UDF!");
  google::protobuf::Any any;
  any.PackFrom(std::move(bin_response));
  SerializeDelimitedToFileDescriptor(any, fd);
  return 0;
}
