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

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "src/roma/byob/benchmark/traffic_generator.h"

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::SetFlag(&FLAGS_mode, "byob");

  using privacy_sandbox::server_common::byob::TrafficGenerator;
  return TrafficGenerator::Run().ok() ? 0 : 1;
}
