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

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "src/roma/gvisor/udf/kv.pb.h"

using privacy_sandbox::server_common::gvisor::GetValuesRequest;
using privacy_sandbox::server_common::gvisor::GetValuesResponse;

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cout << "Not enough arguments!";
    return -1;
  }
  GetValuesRequest bin_request;
  bin_request.ParseFromFileDescriptor(STDIN_FILENO);
  int32_t write_fd;
  CHECK(absl::SimpleAtoi(argv[1], &write_fd))
      << "Conversion of write file descriptor string to int failed";
  GetValuesResponse bin_response;
  bin_response.set_greeting("I am a new UDF!");

  bin_response.SerializeToFileDescriptor(write_fd);
  close(write_fd);
  return 0;
}
