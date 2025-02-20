/*
 * Copyright 2025 Google LLC
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
 * //src/roma/benchmark:duration_benchmark \
 * --test_output=all 2>&1 | grep -Ev "sandbox.cc|monitor_base.cc|sandbox2.cc"
 */

#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/time/time.h"

namespace google::scp::roma::benchmark {

void BM_DirectDurationConstruction(::benchmark::State& state) {
  const int64_t milliseconds = 5000;
  for (auto _ : state) {
    ::benchmark::DoNotOptimize(absl::Milliseconds(milliseconds));
  }
}

void BM_StringToIntToDuration(::benchmark::State& state) {
  const std::string milliseconds_str = "5000";
  for (auto _ : state) {
    int64_t ms;
    CHECK(absl::SimpleAtoi(milliseconds_str, &ms));
    ::benchmark::DoNotOptimize(absl::Milliseconds(ms));
  }
}

void BM_ParseDuration(::benchmark::State& state) {
  const std::string duration_str = "5000ms";
  for (auto _ : state) {
    absl::Duration duration;
    ::benchmark::DoNotOptimize(absl::ParseDuration(duration_str, &duration));
  }
}

BENCHMARK(BM_DirectDurationConstruction);
BENCHMARK(BM_StringToIntToDuration);
BENCHMARK(BM_ParseDuration);

}  // namespace google::scp::roma::benchmark

// Run the benchmark
BENCHMARK_MAIN();
