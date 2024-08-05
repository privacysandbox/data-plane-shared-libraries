/*
 * Copyright 2022 Google LLC
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

#include <benchmark/benchmark.h>

#include "src/roma/benchmark/roma_benchmark.h"

using google::scp::roma::benchmark::InputsType;
using google::scp::roma::benchmark::RomaBenchmarkSuite;
using google::scp::roma::benchmark::TestConfiguration;

static void RomaPayloadTest(benchmark::State& state) {
  TestConfiguration test_configuration = {
      .inputs_type = InputsType::kSimpleString,
      .input_payload_in_byte = static_cast<size_t>(state.range(0)),
      .workers = 1,
      .queue_size = 10,
      .request_threads = 1,
      .batch_size = 1,
      .requests_per_thread = 10'000,
  };
  for (auto _ : state) {
    RomaBenchmarkSuite(test_configuration);
  }
}

static void RomaJsonInputParsingTest(benchmark::State& state) {
  TestConfiguration test_configuration = {
      .inputs_type = InputsType::kNestedJsonString,
      .input_json_nested_depth = static_cast<size_t>(state.range(0)),
      .workers = 1,
      .queue_size = 100,
      .request_threads = 1,
      .batch_size = 1,
      .requests_per_thread = 10'000,
  };
  for (auto _ : state) {
    RomaBenchmarkSuite(test_configuration);
  }
}

static void RomaWorkerAndQueueTest(benchmark::State& state) {
  TestConfiguration test_configuration = {
      .inputs_type = InputsType::kSimpleString,
      .input_payload_in_byte = 500'000,
      .workers = static_cast<size_t>(state.range(0)),
      .queue_size = static_cast<size_t>(state.range(1)),
      .request_threads = 1,
      .batch_size = 1,
      .requests_per_thread = 10'000,
  };
  for (auto _ : state) {
    RomaBenchmarkSuite(test_configuration);
  }
}

static void CustomArgumentsForWorkerAndQueueTest(
    benchmark::internal::Benchmark* b) {
  // worker size: {1, 4, 16}
  // queue size: {1, 10, 100, 1000}
  for (int i = 1; i <= 16; i *= 4) {
    for (int j = 1; j <= 1000; j *= 10) {
      b->Args({i, j});
    }
  }
}

static void RomaBatchSizeTest(benchmark::State& state) {
  TestConfiguration test_configuration = {
      .inputs_type = InputsType::kSimpleString,
      .input_payload_in_byte = 500'000,
      .workers = 16,
      .queue_size = 10,
      .request_threads = 1,
      .batch_size = static_cast<size_t>(state.range(0)),
      .requests_per_thread = 10'000,
  };
  for (auto _ : state) {
    RomaBenchmarkSuite(test_configuration);
  }
}

// Register the function as a benchmark
BENCHMARK(RomaPayloadTest)->RangeMultiplier(10)->Range(1, 1024 * 1024 * 10);
BENCHMARK(RomaJsonInputParsingTest)->RangeMultiplier(10)->Range(1, 1000);
BENCHMARK(RomaJsonInputParsingTest)->RangeMultiplier(10)->Range(1, 100'000);
BENCHMARK(RomaPayloadTest)->DenseRange(0, 1024 * 1024, 1024 * 500);
BENCHMARK(RomaWorkerAndQueueTest)->Apply(CustomArgumentsForWorkerAndQueueTest);
BENCHMARK(RomaBatchSizeTest)->RangeMultiplier(10)->Range(1, 100);

// Run the benchmark
BENCHMARK_MAIN();
