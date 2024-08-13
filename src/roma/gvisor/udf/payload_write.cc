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

using ::privacy_sandbox::server_common::gvisor::GeneratePayloadRequest;
using ::privacy_sandbox::server_common::gvisor::GeneratePayloadResponse;

int main(int argc, char* argv[]) {
  GeneratePayloadRequest req;
  req.ParseFromIstream(&std::cin);

  GeneratePayloadResponse response;
  auto* payloads = response.mutable_payloads();
  payloads->Reserve(req.element_count());
  for (auto i = 0; i < req.element_count(); ++i) {
    payloads->Add(std::string(req.element_size(), 'a'));
  }
  response.SerializeToOstream(&std::cout);
  return 0;
}
