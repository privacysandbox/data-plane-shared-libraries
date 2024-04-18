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
 * //src/roma/benchmark/serde:serialize_benchmark \
 * --test_output=all 2>&1 | grep -Ev "sandbox.cc|monitor_base.cc|sandbox2.cc"
 */
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "src/roma/benchmark/serde/benchmark_service.pb.h"
#include "src/roma/benchmark/serde/serde_utils.h"
#include "src/roma/benchmark/test_code.h"

namespace google::scp::roma::benchmark::proto {

using privacy_sandbox::server_common::BenchmarkRequest;

void SerializeProtobufBenchmark(::benchmark::State& state,
                                std::string_view path) {
  BenchmarkRequest req = GetProtoFromPath(path);
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(req.SerializeAsString());
  }
}

void BM_RomaJsSerializeSmallProtobuf(::benchmark::State& state) {
  RunRomaJsBenchmark(
      state, google::scp::roma::benchmark::kCodeSerializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameSerializeFunc, kSmallJsonPath);
}

void BM_RomaJsSerializeMediumProtobuf(::benchmark::State& state) {
  RunRomaJsBenchmark(
      state, google::scp::roma::benchmark::kCodeSerializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameSerializeFunc, kMediumJsonPath);
}

void BM_RomaJsSerializeLargeProtobuf(::benchmark::State& state) {
  RunRomaJsBenchmark(
      state, google::scp::roma::benchmark::kCodeSerializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameSerializeFunc, kLargeJsonPath);
}

void BM_CppSerializeSmallProtobuf(::benchmark::State& state) {
  SerializeProtobufBenchmark(state, kSmallProtoPath);
}

void BM_CppSerializeMediumProtobuf(::benchmark::State& state) {
  SerializeProtobufBenchmark(state, kMediumProtoPath);
}

void BM_CppSerializeLargeProtobuf(::benchmark::State& state) {
  SerializeProtobufBenchmark(state, kLargeProtoPath);
}

BENCHMARK(BM_RomaJsSerializeSmallProtobuf);
BENCHMARK(BM_RomaJsSerializeMediumProtobuf);
BENCHMARK(BM_RomaJsSerializeLargeProtobuf);
BENCHMARK(BM_CppSerializeSmallProtobuf);
BENCHMARK(BM_CppSerializeMediumProtobuf);
BENCHMARK(BM_CppSerializeLargeProtobuf);

}  // namespace google::scp::roma::benchmark::proto

// Run the benchmark
BENCHMARK_MAIN();
