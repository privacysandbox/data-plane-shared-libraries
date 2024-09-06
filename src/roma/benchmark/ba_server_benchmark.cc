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
 * //src/roma/benchmark:ba_server_benchmark \
 * --test_output=all 2>&1 | fgrep -v sandbox2.cc
 */

#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "src/roma/benchmark/fake_ba_server.h"
#include "src/roma/benchmark/test_code.h"
#include "src/roma/config/config.h"

namespace {

using google::scp::roma::Config;
using google::scp::roma::benchmark::DispatchRequest;
using google::scp::roma::benchmark::FakeBaServer;

constexpr std::string_view kVersionString = "v1";

void LoadCodeBenchmark(std::string_view code, benchmark::State& state) {
  Config config;
  config.number_of_workers = 10;
  FakeBaServer server(std::move(config));

  // If the code is being padded with extra bytes then add a comment at the end
  // and fill it with extra zeroes.
  std::string padded_code = std::string(code);
  if (const int extra_padding_bytes = state.range(0); extra_padding_bytes > 0) {
    constexpr std::string_view extra_prefix = " // ";
    const int total_size =
        padded_code.size() + extra_prefix.size() + extra_padding_bytes;
    padded_code.reserve(total_size);
    padded_code.append(extra_prefix);
    padded_code.resize(total_size, '0');
  }

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    server.LoadSync(kVersionString, padded_code);
  }
  state.SetItemsProcessed(state.iterations());
  state.SetBytesProcessed(state.iterations() * padded_code.length());
}

void ExecuteCodeBenchmark(std::string_view code, std::string_view handler_name,
                          benchmark::State& state) {
  FakeBaServer server(Config{});
  server.LoadSync(kVersionString, std::string{code});

  // The same request will be used multiple times: the batch will be full of
  // identical code to run.
  DispatchRequest request = {
      .id = "id",
      .version_string = std::string(kVersionString),
      .handler_name = std::string{handler_name},
      // TODO(b/305957393): Right now no input is passed to these calls, add
      // this!
      // .input = { std::make_shared(my_input_value_one) },
  };

  const int batch_size = state.range(0);
  std::vector<DispatchRequest> batch(batch_size, request);

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    server.BatchExecute(batch);
  }
  state.SetItemsProcessed(batch_size);
}

void BM_LoadCodeWithPadding(benchmark::State& state) {
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
BENCHMARK(BM_LoadCodeWithPadding)
    ->ArgsProduct({
        // Pad with this many extra bytes.
        {0, 128, 512, 1024, 10'000, 20'000, 50'000, 100'000, 200'000, 500'000},
    })
    ->ArgNames({"padding_in_bytes"});
BENCHMARK(BM_LoadGoogleAdManagerGenerateBid)
    ->ArgsProduct({
        {0},  // No need to pad this code with extra bytes.
    })
    ->ArgNames({"padding_in_bytes"});

// Run these benchmarks with {1, 10, 100} requests in each batch.
BENCHMARK(BM_ExecuteHelloWorld)->RangeMultiplier(10)->Range(1, 100);
BENCHMARK(BM_ExecutePrimeSieve)->RangeMultiplier(10)->Range(1, 100);

// Run the benchmark
BENCHMARK_MAIN();
