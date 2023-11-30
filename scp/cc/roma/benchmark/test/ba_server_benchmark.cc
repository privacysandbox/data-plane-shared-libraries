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
 *
 * Example command to run this (the grep is necessary to avoid noisy log
 * output):
 *
 * builders/tools/bazel-debian run \
 * //scp/cc/roma/benchmark/test:ba_server_benchmark \
 * --test_output=all 2>&1 | fgrep -v sandbox2.cc
 */

#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "roma/benchmark/src/fake_ba_server.h"
#include "roma/benchmark/test/test_code.h"
#include "roma/config/src/config.h"

namespace {

using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::benchmark::DispatchRequest;
using google::scp::roma::benchmark::FakeBaServer;

constexpr std::string_view kVersionString = "v1";

void LoadCodeBenchmark(std::string code, benchmark::State& state) {
  const Config config;
  FakeBaServer server(config);

  // If the code is being padded with extra bytes then add a comment at the end
  // and fill it with extra zeroes.
  const int extra_padding_bytes = state.range(1);
  if (extra_padding_bytes > 0) {
    std::string padding = " // ";
    padding += std::string('0', extra_padding_bytes);
    code += padding;
  }

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  const int number_of_loads = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_loads; ++i) {
      server.LoadSync(kVersionString, code);
    }
  }
  state.SetItemsProcessed(number_of_loads);
  state.SetBytesProcessed(number_of_loads * code.length());
}

void ExecuteCodeBenchmark(std::string code, std::string handler_name,
                          benchmark::State& state) {
  const Config config;
  FakeBaServer server(config);
  server.LoadSync(kVersionString, code);

  // The same request will be used multiple times: the batch will be full of
  // identical code to run.
  DispatchRequest request = {
      .id = "id",
      .version_string = kVersionString,
      .handler_name = handler_name,
      // TODO(b/305957393): Right now no input is passed to these calls, add
      // this!
      // .input = { std::make_shared(my_input_value_one) },
  };

  std::vector<DispatchRequest> batch;
  const int batch_size = state.range(0);
  for (int i = 0; i < batch_size; ++i) {
    batch.push_back(request);
  }

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    server.BatchExecute(batch);
  }
  state.SetItemsProcessed(batch_size);
}

void BM_LoadHelloWorld(benchmark::State& state) {
  LoadCodeBenchmark(google::scp::roma::benchmark::kCodeHelloWorld, state);
}

void BM_LoadGoogleAdManagerGenerateBid(benchmark::State& state) {
  LoadCodeBenchmark(
      google::scp::roma::benchmark::kCodeGoogleAdManagerGenerateBid, state);
}

void BM_ExecuteHelloWorld(benchmark::State& state) {
  ExecuteCodeBenchmark(google::scp::roma::benchmark::kCodeHelloWorld,
                       google::scp::roma::benchmark::kHandlerNameHelloWorld,
                       state);
}

void BM_ExecutePrimeSieve(benchmark::State& state) {
  ExecuteCodeBenchmark(google::scp::roma::benchmark::kCodePrimeSieve,
                       google::scp::roma::benchmark::kHandlerNamePrimeSieve,
                       state);
}

}  // namespace

// Register the function as a benchmark
BENCHMARK(BM_LoadHelloWorld)
    ->ArgsProduct({
        {1, 10, 100},         // Run this many loads of the code.
        {0, 128, 512, 1024},  // Pad with this many extra bytes.
    });
BENCHMARK(BM_LoadGoogleAdManagerGenerateBid)
    ->ArgsProduct({
        {1, 10, 100},  // Run this many loads of the code.
        {0},           // No need to pad this code with extra bytes.
    });

// Run these benchmarks with {1, 10, 100} requests in each batch.
BENCHMARK(BM_ExecuteHelloWorld)->RangeMultiplier(10)->Range(1, 100);
BENCHMARK(BM_ExecutePrimeSieve)->RangeMultiplier(10)->Range(1, 100);

// Run the benchmark
BENCHMARK_MAIN();
