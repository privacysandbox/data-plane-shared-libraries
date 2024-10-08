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
#include "src/roma/byob/host/callback.pb.h"
#include "src/roma/byob/sample_udf/sample_callback.pb.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

using ::privacy_sandbox::roma_byob::example::WriteCallbackPayloadRequest;
using ::privacy_sandbox::roma_byob::example::WriteCallbackPayloadResponse;
using ::privacy_sandbox::server_common::byob::Callback;
using ::privacy_sandbox::server_common::byob::CallbackWriteRequest;
using ::privacy_sandbox::server_common::byob::CallbackWriteResponse;

WriteCallbackPayloadRequest ReadRequestFromFd(
    google::protobuf::io::FileInputStream& stream) {
  WriteCallbackPayloadRequest req;
  google::protobuf::Any any;
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&any, &stream,
                                                           nullptr);
  any.UnpackTo(&req);
  return req;
}

void WriteResponseToFd(int fd, WriteCallbackPayloadResponse resp) {
  google::protobuf::Any any;
  any.PackFrom(std::move(resp));
  google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  google::protobuf::io::FileInputStream stream(fd);
  WriteCallbackPayloadRequest req = ReadRequestFromFd(stream);
  CallbackWriteRequest request;
  request.set_element_size(req.element_size());
  request.set_element_count(req.element_count());
  {
    Callback callback;
    callback.set_function_name("example");
    request.SerializeToString(
        callback.mutable_io_proto()->mutable_input_bytes());
    google::protobuf::Any any;
    any.PackFrom(std::move(callback));
    google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
  }
  Callback callback;
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&callback, &stream,
                                                           nullptr);
  CallbackWriteResponse response;
  response.ParseFromString(callback.io_proto().output_bytes());

  WriteCallbackPayloadResponse resp;
  int64_t payload_size = 0;
  for (const auto& p : response.payloads()) {
    payload_size += p.size();
  }
  resp.set_payload_size(payload_size);
  WriteResponseToFd(fd, std::move(resp));
  return 0;
}
