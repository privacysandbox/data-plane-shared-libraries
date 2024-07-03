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
 *
 */

#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <string>
#include <string_view>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "flatbuffers/flatbuffer_builder.h"
#include "src/roma/benchmark/flatbuffers/data_structures_generated.h"

ABSL_FLAG(int64_t, num_entries, 10'000, "Number of map entries");
ABSL_FLAG(std::string, output_file, "", "Data output file");
ABSL_FLAG(bool, generate_js, false, "Generate JavaScript output");
ABSL_FLAG(bool, verbose, false, "Verbose output");

namespace {

std::vector<std::pair<std::string, std::string>> CreateKVData(
    int64_t length, int32_t val_size = 100, int64_t start_value = 1'000'000) {
  std::vector<std::pair<std::string, std::string>> kvdata;
  kvdata.reserve(length);
  const std::string filler = std::string(val_size, '.');
  for (int64_t i = 0; i < length; ++i) {
    const std::string key = absl::StrFormat("k-%026d", i + start_value);
    const std::string value = absl::StrCat("v-", key.substr(1), filler, "--v");
    kvdata.push_back(std::make_pair(key, value));
  }
  return kvdata;
}

flatbuffers::FlatBufferBuilder GenerateKeyValueData() {
  flatbuffers::FlatBufferBuilder builder(1 << 24);

  std::vector<std::pair<std::string, std::string>> kvdata =
      CreateKVData(absl::GetFlag(FLAGS_num_entries), 100, 10'000'000'000UL);
  std::vector<flatbuffers::Offset<privacysandbox_roma_benchmarks::KeyValue>>
      keyvalues;
  keyvalues.reserve(kvdata.size());
  std::transform(kvdata.begin(), kvdata.end(), back_inserter(keyvalues),
                 [&builder](const auto& kvpair) {
                   auto k = builder.CreateString(kvpair.first);
                   auto v = builder.CreateString(kvpair.second);
                   return privacysandbox_roma_benchmarks::CreateKeyValue(
                       builder, k, v);
                 });
  auto kv_fb = builder.CreateVector(keyvalues);
  auto kvdata_fb = privacysandbox_roma_benchmarks::CreateKVData(builder, kv_fb);
  builder.Finish(kvdata_fb);
  return builder;
}

}  // namespace

int main(int argc, char* argv[]) {
  absl::SetProgramUsageMessage(
      "Generate flatbuffers-format data structure for benchmarking purposes.");
  absl::ParseCommandLine(argc, argv);

  if (absl::GetFlag(FLAGS_num_entries) < 1) {
    std::cerr << "num_entries must be a positive integer" << std::endl;
    return -1;
  }
  flatbuffers::FlatBufferBuilder builder = GenerateKeyValueData();

  std::string_view data_strview(
      reinterpret_cast<const char*>(builder.GetBufferPointer()),
      builder.GetBufferSpan().size_bytes());
  const std::string out_fname(absl::GetFlag(FLAGS_output_file));
  std::ofstream f(out_fname, std::ios::binary);
  if (absl::GetFlag(FLAGS_generate_js)) {
    f << "const fbs_data_b64 = \"" << absl::Base64Escape(data_strview)
      << "\";\n";
  } else {
    f << data_strview;
  }
  if (absl::GetFlag(FLAGS_verbose)) {
    std::cerr << "size: " << builder.GetBufferSpan().size() << std::endl;
    std::cerr << "size_bytes: " << builder.GetBufferSpan().size_bytes()
              << std::endl;
  }
}
