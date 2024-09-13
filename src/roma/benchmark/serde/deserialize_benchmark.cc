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
 * Example command to run this (the grep is necessary to avoid noisy log
 * output):
 *
 * builders/tools/bazel-debian run \
 * //src/roma/benchmark/serde:deserialize_benchmark \
 * --test_output=all 2>&1 | grep -Ev "sandbox.cc|monitor_base.cc|sandbox2.cc"
 */
#include <fstream>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>
#include <nlohmann/json.hpp>

#include "src/roma/benchmark/serde/benchmark_service.pb.h"
#include "src/roma/benchmark/serde/serde_utils.h"
#include "src/roma/benchmark/test_code.h"

namespace google::scp::roma::benchmark::proto {

using privacy_sandbox::benchmark::BenchmarkRequest;

void DeserializeProtobufBenchmark(::benchmark::State& state,
                                  std::string_view path) {
  BenchmarkRequest inputReq = GetProtoFromPath(path);

  std::string serializedInput = inputReq.SerializeAsString();
  BenchmarkRequest outputReq;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(outputReq.ParseFromString(serializedInput));
  }
}

void DeserializeJsonBenchmark(::benchmark::State& state,
                              std::string_view path) {
  std::ifstream input_file(path.data());
  std::stringstream buffer;
  buffer << input_file.rdbuf();
  std::string serializedInput = buffer.str();
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(nlohmann::json::parse(serializedInput));
  }
}

void BM_RomaJsDeserializeSmallProtobuf(::benchmark::State& state) {
  RunRomaJsBenchmark(state,
                     google::scp::roma::benchmark::kCodeDeserializeProtobuf,
                     google::scp::roma::benchmark::kHandlerNameDeserializeFunc,
                     kSmallJsonPath);
}

void BM_RomaJsDeserializeMediumProtobuf(::benchmark::State& state) {
  RunRomaJsBenchmark(state,
                     google::scp::roma::benchmark::kCodeDeserializeProtobuf,
                     google::scp::roma::benchmark::kHandlerNameDeserializeFunc,
                     kMediumJsonPath);
}

void BM_RomaJsDeserializeLargeProtobuf(::benchmark::State& state) {
  RunRomaJsBenchmark(state,
                     google::scp::roma::benchmark::kCodeDeserializeProtobuf,
                     google::scp::roma::benchmark::kHandlerNameDeserializeFunc,
                     kLargeJsonPath);
}

void BM_RomaJsDeserializeSmallJson(::benchmark::State& state) {
  RunRomaJsBenchmark(state, google::scp::roma::benchmark::kCodeDeserializeJson,
                     google::scp::roma::benchmark::kHandlerNameDeserializeFunc,
                     kSmallJsonPath);
}

void BM_RomaJsDeserializeMediumJson(::benchmark::State& state) {
  RunRomaJsBenchmark(state, google::scp::roma::benchmark::kCodeDeserializeJson,
                     google::scp::roma::benchmark::kHandlerNameDeserializeFunc,
                     kMediumJsonPath);
}

void BM_RomaJsDeserializeLargeJson(::benchmark::State& state) {
  RunRomaJsBenchmark(state, google::scp::roma::benchmark::kCodeDeserializeJson,
                     google::scp::roma::benchmark::kHandlerNameDeserializeFunc,
                     kLargeJsonPath);
}

void BM_CppDeserializeSmallProtobuf(::benchmark::State& state) {
  DeserializeProtobufBenchmark(state, kSmallProtoPath);
}

void BM_CppDeserializeMediumProtobuf(::benchmark::State& state) {
  DeserializeProtobufBenchmark(state, kMediumProtoPath);
}

void BM_CppDeserializeLargeProtobuf(::benchmark::State& state) {
  DeserializeProtobufBenchmark(state, kLargeProtoPath);
}

void BM_CppDeserializeSmallJson(::benchmark::State& state) {
  DeserializeJsonBenchmark(state, kSmallJsonPath);
}

void BM_CppDeserializeMediumJson(::benchmark::State& state) {
  DeserializeJsonBenchmark(state, kMediumJsonPath);
}

void BM_CppDeserializeLargeJson(::benchmark::State& state) {
  DeserializeJsonBenchmark(state, kLargeJsonPath);
}

BENCHMARK(BM_RomaJsDeserializeSmallProtobuf);
BENCHMARK(BM_RomaJsDeserializeMediumProtobuf);
BENCHMARK(BM_RomaJsDeserializeLargeProtobuf);
BENCHMARK(BM_RomaJsDeserializeSmallJson);
BENCHMARK(BM_RomaJsDeserializeMediumJson);
BENCHMARK(BM_RomaJsDeserializeLargeJson);
BENCHMARK(BM_CppDeserializeSmallProtobuf);
BENCHMARK(BM_CppDeserializeMediumProtobuf);
BENCHMARK(BM_CppDeserializeLargeProtobuf);
BENCHMARK(BM_CppDeserializeSmallJson);
BENCHMARK(BM_CppDeserializeMediumJson);
BENCHMARK(BM_CppDeserializeLargeJson);

}  // namespace google::scp::roma::benchmark::proto

// Run the benchmark
BENCHMARK_MAIN();
