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

enum class DataLayout {
  kKVStrData,
  kKVKeyData,
  kKVDataParallel,
};

ABSL_FLAG(int64_t, num_entries, 10'000, "Number of map entries");
ABSL_FLAG(std::string, output_file, "", "Data output file");
ABSL_FLAG(bool, generate_js, false, "Generate JavaScript output");
ABSL_FLAG(bool, verbose, false, "Verbose output");
ABSL_FLAG(DataLayout, data_layout, DataLayout::kKVStrData, "Data layout");

// AbslParseFlag converts from a string to DataLayout.
// Must be in same namespace as DataLayout.

bool AbslParseFlag(absl::string_view text, DataLayout* layout,
                   std::string* error) {
  if (text == "KVStrData") {
    *layout = DataLayout::kKVStrData;
    return true;
  }
  if (text == "KVKeyData") {
    *layout = DataLayout::kKVKeyData;
    return true;
  }
  if (text == "KVDataParallel") {
    *layout = DataLayout::kKVDataParallel;
    return true;
  }
  *error = "unknown value for enumeration";
  return false;
}

// AbslUnparseFlag converts from an DataLayout to a string.
// Must be in same namespace as DataLayout.

// Returns a textual flag value corresponding to the DataLayout `mode`.
std::string AbslUnparseFlag(DataLayout mode) {
  switch (mode) {
    case DataLayout::kKVStrData:
      return "KVStrData";
    case DataLayout::kKVKeyData:
      return "KVKeyData";
    case DataLayout::kKVDataParallel:
      return "KVDataParallel";
    default:
      return absl::StrCat(mode);
  }
}

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

flatbuffers::FlatBufferBuilder GenerateKVStrData(
    const std::vector<std::pair<std::string, std::string>>& kvdata) {
  using T = privacysandbox_roma_benchmarks::KeyValueStr;
  flatbuffers::FlatBufferBuilder builder(1UL << 32);

  std::vector<flatbuffers::Offset<T>> keyvalues;
  keyvalues.reserve(kvdata.size());
  std::transform(kvdata.begin(), kvdata.end(), back_inserter(keyvalues),
                 [&builder](const auto& kvpair) {
                   auto k = builder.CreateString(kvpair.first);
                   auto v = builder.CreateString(kvpair.second);
                   return privacysandbox_roma_benchmarks::CreateKeyValueStr(
                       builder, k, v);
                 });
  auto kv_fb = builder.CreateVector(keyvalues);
  auto kvdata_fb =
      privacysandbox_roma_benchmarks::CreateKVStrData(builder, kv_fb);
  builder.Finish(kvdata_fb);
  return builder;
}

flatbuffers::FlatBufferBuilder GenerateKVKeyData(
    const std::vector<std::pair<std::string, std::string>>& kvdata) {
  using T = privacysandbox_roma_benchmarks::KeyValueKey;
  flatbuffers::FlatBufferBuilder builder(1UL << 32);

  std::vector<flatbuffers::Offset<T>> keyvalues;
  keyvalues.reserve(kvdata.size());
  std::transform(kvdata.begin(), kvdata.end(), back_inserter(keyvalues),
                 [&builder](const auto& kvpair) {
                   auto k = builder.CreateString(kvpair.first);
                   auto v = builder.CreateString(kvpair.second);
                   return privacysandbox_roma_benchmarks::CreateKeyValueKey(
                       builder, k, v);
                 });
  auto kv_fb = builder.CreateVector(keyvalues);
  auto kvdata_fb =
      privacysandbox_roma_benchmarks::CreateKVKeyData(builder, kv_fb);
  builder.Finish(kvdata_fb);
  return builder;
}

flatbuffers::FlatBufferBuilder GenerateKVDataParallel(
    const std::vector<std::pair<std::string, std::string>>& kvdata) {
  using T = flatbuffers::String;
  flatbuffers::FlatBufferBuilder builder(1UL << 32);

  std::vector<flatbuffers::Offset<T>> keys;
  std::vector<flatbuffers::Offset<T>> values;
  keys.reserve(kvdata.size());
  values.reserve(kvdata.size());
  std::transform(kvdata.begin(), kvdata.end(), back_inserter(keys),
                 [&builder](const auto& kvpair) {
                   return builder.CreateString(kvpair.first);
                 });
  std::transform(kvdata.begin(), kvdata.end(), back_inserter(values),
                 [&builder](const auto& kvpair) {
                   return builder.CreateString(kvpair.second);
                 });
  auto keys_fb = builder.CreateVector(keys);
  auto values_fb = builder.CreateVector(values);
  auto kvdata_fb = privacysandbox_roma_benchmarks::CreateKVDataParallel(
      builder, keys_fb, values_fb);
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
  const std::vector<std::pair<std::string, std::string>> kvdata =
      CreateKVData(absl::GetFlag(FLAGS_num_entries), 100, 10'000'000'000UL);
  flatbuffers::FlatBufferBuilder builder;
  switch (absl::GetFlag(FLAGS_data_layout)) {
    case DataLayout::kKVStrData:
      builder = GenerateKVStrData(kvdata);
      break;
    case DataLayout::kKVKeyData:
      builder = GenerateKVKeyData(kvdata);
      break;
    case DataLayout::kKVDataParallel:
      builder = GenerateKVDataParallel(kvdata);
      break;
    default:
      std::cerr << "unsupported data layout" << std::endl;
      return -1;
  }

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
