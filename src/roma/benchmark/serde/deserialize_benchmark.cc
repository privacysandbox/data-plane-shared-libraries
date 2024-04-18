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
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "src/roma/benchmark/serde/benchmark_service.pb.h"
#include "src/roma/benchmark/serde/serde_utils.h"
#include "src/roma/benchmark/test_code.h"

namespace google::scp::roma::benchmark::proto {

using privacy_sandbox::server_common::BenchmarkRequest;

void DeserializeProtobufBenchmark(::benchmark::State& state,
                                  std::string_view path) {
  BenchmarkRequest inputReq = GetProtoFromPath(path);

  std::string serializedInput = inputReq.SerializeAsString();
  BenchmarkRequest outputReq;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(outputReq.ParseFromString(serializedInput));
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

void BM_CppDeserializeSmallProtobuf(::benchmark::State& state) {
  DeserializeProtobufBenchmark(state, kSmallProtoPath);
}

void BM_CppDeserializeMediumProtobuf(::benchmark::State& state) {
  DeserializeProtobufBenchmark(state, kMediumProtoPath);
}

void BM_CppDeserializeLargeProtobuf(::benchmark::State& state) {
  DeserializeProtobufBenchmark(state, kLargeProtoPath);
}

BENCHMARK(BM_RomaJsDeserializeSmallProtobuf);
BENCHMARK(BM_RomaJsDeserializeMediumProtobuf);
BENCHMARK(BM_RomaJsDeserializeLargeProtobuf);
BENCHMARK(BM_CppDeserializeSmallProtobuf);
BENCHMARK(BM_CppDeserializeMediumProtobuf);
BENCHMARK(BM_CppDeserializeLargeProtobuf);

}  // namespace google::scp::roma::benchmark::proto

// Run the benchmark
BENCHMARK_MAIN();
