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

using ::privacy_sandbox::roma_byob::example::ReadCallbackPayloadRequest;
using ::privacy_sandbox::roma_byob::example::ReadCallbackPayloadResponse;
using ::privacy_sandbox::server_common::byob::Callback;
using ::privacy_sandbox::server_common::byob::CallbackReadRequest;
using ::privacy_sandbox::server_common::byob::CallbackReadResponse;

ReadCallbackPayloadRequest ReadRequestFromFd(
    google::protobuf::io::FileInputStream& stream) {
  ReadCallbackPayloadRequest req;
  google::protobuf::Any any;
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&any, &stream,
                                                           nullptr);
  any.UnpackTo(&req);
  return req;
}

void WriteResponseToFd(int fd, ReadCallbackPayloadResponse resp) {
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
  ReadCallbackPayloadRequest req = ReadRequestFromFd(stream);
  CallbackReadRequest request;
  std::string payload(req.element_size(), char(10));
  auto payloads = request.mutable_payloads();
  payloads->Reserve(req.element_count());
  for (auto i = 0; i < req.element_count(); ++i) {
    payloads->Add(payload.data());
  }
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
  CallbackReadResponse response;
  response.ParseFromString(callback.io_proto().output_bytes());

  ReadCallbackPayloadResponse resp;
  resp.set_payload_size(response.payload_size());
  WriteResponseToFd(fd, std::move(resp));
  return 0;
}
