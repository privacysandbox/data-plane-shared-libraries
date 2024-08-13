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

using ::privacy_sandbox::server_common::gvisor::SampleRequest;
using ::privacy_sandbox::server_common::gvisor::SampleResponse;

int main(int argc, char* argv[]) {
  SampleRequest bin_request;
  bin_request.ParseFromIstream(&std::cin);
  SampleResponse bin_response;
  bin_response.set_greeting("I am a new UDF!");
  bin_response.SerializeToOstream(&std::cout);
  return 0;
}
