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

#include <string>

#include <benchmark/benchmark.h>

#include "src/core/common/uuid/uuid.h"

#include "uuid_to_string.h"

namespace {

using google::scp::core::common::Uuid;
namespace T = google::scp::core::common::test;

const Uuid uuid = Uuid::GenerateUuid();

void BM_UuidToString(benchmark::State& state) {
  const int number_of_calls = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      auto uuid_string = ToString(uuid);
      benchmark::DoNotOptimize(uuid_string);
    }
  }
}

BENCHMARK(BM_UuidToString)->RangeMultiplier(10)->Range(10, 1000);

void BM_UuidToString_AbslFormat(benchmark::State& state) {
  const int number_of_calls = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      auto uuid_string = T::ToStringAbslFormat(uuid);
      benchmark::DoNotOptimize(uuid_string);
    }
  }
}

BENCHMARK(BM_UuidToString_AbslFormat)->RangeMultiplier(10)->Range(10, 1000);

void BM_UuidToString_AbslAppend(benchmark::State& state) {
  const int number_of_calls = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      auto uuid_string = T::ToStringAbslAppend(uuid);
      benchmark::DoNotOptimize(uuid_string);
    }
  }
}

BENCHMARK(BM_UuidToString_AbslAppend)->RangeMultiplier(10)->Range(10, 1000);

void BM_UuidToString_HexLookupMap(benchmark::State& state) {
  const int number_of_calls = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      auto uuid_string = T::ToStringFn(uuid, T::AppendHexLookupMap);
      benchmark::DoNotOptimize(uuid_string);
    }
  }
}

BENCHMARK(BM_UuidToString_HexLookupMap)->RangeMultiplier(10)->Range(10, 1000);

void BM_UuidToString_AppendHexByte(benchmark::State& state) {
  const int number_of_calls = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      auto uuid_string = T::ToStringFn(uuid, T::AppendHexByte);
      benchmark::DoNotOptimize(uuid_string);
    }
  }
}

BENCHMARK(BM_UuidToString_AppendHexByte)->RangeMultiplier(10)->Range(10, 1000);

}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
