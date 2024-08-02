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

#include <unistd.h>

#include <iostream>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/gvisor/host/callback.pb.h"
#include "src/roma/gvisor/udf/sample.pb.h"
#include "src/roma/gvisor/udf/sample_callback.pb.h"

using ::privacy_sandbox::server_common::gvisor::Callback;
using ::privacy_sandbox::server_common::gvisor::CallbackWriteRequest;
using ::privacy_sandbox::server_common::gvisor::CallbackWriteResponse;
using ::privacy_sandbox::server_common::gvisor::WriteCallbackPayloadRequest;
using ::privacy_sandbox::server_common::gvisor::WriteCallbackPayloadResponse;

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 3) {
    LOG(ERROR) << "Not enough arguments!";
    return -1;
  }
  int comms_fd;
  CHECK(absl::SimpleAtoi(argv[2], &comms_fd))
      << "Conversion of comms file descriptor string to int failed";
  WriteCallbackPayloadRequest req;
  req.ParseFromFileDescriptor(STDIN_FILENO);
  CallbackWriteRequest request;
  request.set_element_size(req.element_size());
  request.set_element_count(req.element_count());

  Callback callback;
  callback.set_function_name("example");
  request.SerializeToString(callback.mutable_io_proto()->mutable_input_bytes());
  CHECK(google::protobuf::util::SerializeDelimitedToFileDescriptor(callback,
                                                                   comms_fd));
  google::protobuf::io::FileInputStream input(comms_fd);
  CHECK(google::protobuf::util::ParseDelimitedFromZeroCopyStream(
      &callback, &input, nullptr));
  CallbackWriteResponse response;
  response.ParseFromString(callback.io_proto().output_bytes());

  int32_t write_fd;
  CHECK(absl::SimpleAtoi(argv[1], &write_fd))
      << "Conversion of write file descriptor string to int failed";
  WriteCallbackPayloadResponse resp;
  int64_t payload_size = 0;
  for (const auto& p : response.payloads()) {
    payload_size += p.size();
  }
  resp.set_payload_size(payload_size);
  resp.SerializeToFileDescriptor(write_fd);
  ::close(write_fd);
  return 0;
}
