/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <fstream>
#include <iostream>
#include <string>

#include <google/protobuf/text_format.h>
#include <google/protobuf/util/json_util.h>

#include "absl/strings/match.h"
#include "src/roma/benchmark/serde/benchmark_service.pb.h"

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input_file> <output_file>\n";
    return 1;
  }

  std::ifstream inputFile(argv[1]);
  if (!inputFile.is_open()) {
    std::cerr << "Failed to open input file.\n";
    return 1;
  }

  std::string jsonStr((std::istreambuf_iterator<char>(inputFile)),
                      std::istreambuf_iterator<char>());

  privacy_sandbox::benchmark::BenchmarkRequest request;
  if (!google::protobuf::util::JsonStringToMessage(jsonStr, &request).ok()) {
    std::cerr << "Failed to parse to protobuf.\n";
    return -1;
  }

  std::ofstream outFile(argv[2]);

  if (absl::EndsWith(argv[2], ".txtpb")) {
    std::string requestStr;
    google::protobuf::TextFormat::PrintToString(request, &requestStr);
    outFile << requestStr;
  } else if (absl::EndsWith(argv[2], ".pb")) {
    request.SerializeToOstream(&outFile);
  }

  return 0;
}
