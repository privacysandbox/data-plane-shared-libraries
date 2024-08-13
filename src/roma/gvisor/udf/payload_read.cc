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

#include "src/roma/gvisor/udf/sample.pb.h"

using ::privacy_sandbox::server_common::gvisor::ReadPayloadRequest;
using ::privacy_sandbox::server_common::gvisor::ReadPayloadResponse;

int main(int argc, char* argv[]) {
  ReadPayloadRequest req;
  req.ParseFromIstream(&std::cin);

  ReadPayloadResponse response;
  int64_t payload_size = 0;
  for (const auto& p : req.payloads()) {
    payload_size += p.size();
  }
  response.set_payload_size(payload_size);
  response.SerializeToOstream(&std::cout);
  return 0;
}
