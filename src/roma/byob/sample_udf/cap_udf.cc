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

#include <sys/capability.h>

#include <iostream>

#include "absl/cleanup/cleanup.h"
#include "absl/strings/str_cat.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;

void ReadRequestFromFd(int fd) {
  SampleRequest bin_request;
  FileInputStream input(fd);
  ParseDelimitedFromZeroCopyStream(&bin_request, &input, nullptr);
}

void WriteResponseToFd(int fd, SampleResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  ReadRequestFromFd(fd);
  SampleResponse bin_response;
  cap_t caps = cap_get_proc();
  absl::Cleanup caps_cleaner = [caps] { cap_free(caps); };
  if (caps == nullptr) {
    bin_response.set_greeting("Failed to get capabilities.");
  } else if (cap_t empty_caps = cap_init();
             cap_compare(caps, empty_caps) == 0) {
    // All good.
    cap_free(empty_caps);
    bin_response.set_greeting("Empty capabilities set as expected.");
  } else {
    char* cap_str = cap_to_text(caps, nullptr);
    bin_response.set_greeting(
        absl::StrCat("Non-empty capability set: ", cap_str));
  }
  WriteResponseToFd(fd, std::move(bin_response));
  return 0;
}
