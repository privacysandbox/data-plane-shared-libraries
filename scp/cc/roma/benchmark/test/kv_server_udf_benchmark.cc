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
 * //scp/cc/roma/benchmark/test:kv_server_udf_benchmark_test \
 * --test_output=all 2>&1 | fgrep -v sandbox2.cc
 */

#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "roma/benchmark/src/fake_kv_server.h"
#include "roma/config/src/config.h"
#include "roma/config/src/function_binding_object.h"

namespace {

using google::scp::roma::Config;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::benchmark::CodeConfig;
using google::scp::roma::benchmark::FakeKvServer;
using google::scp::roma::proto::FunctionBindingIoProto;

constexpr char kCodeHelloWorld[] = "hello = () => 'Hello world!';";
constexpr char kHandlerNameHelloWorld[] = "hello";

// This JS function is deliberately something that's expensive to run.  The
// code is based on:
// https://www.tutorialspoint.com/using-sieve-of-eratosthenes-to-find-primes-javascript
constexpr char kCodePrimeSieve[] = R"(
      function sieve() {
         // Find all prime numbers less than this:
         const n = 100000;
         // Create a boolean array of size n+1
         const primes = new Array(n + 1).fill(true);
         // Set first two values to false
         primes[0] = false;
         primes[1] = false;
         // Loop through the elements
         for (let i = 2; i <= Math.sqrt(n); i++) {
            if (primes[i]) {
               for (let j = i * i; j <= n; j += i) {
                  primes[j] = false;
               }
            }
         }

         const result = [];
         // Loop through the array from 2 to n
         for (let i = 2; i <= n; i++) {
            if (primes[i]) {
               result.push(i);
            }
         }

         return result;
      }
    )";
constexpr char kHandlerNamePrimeSieve[] = "sieve";

void LoadCodeBenchmark(std::string_view code, std::string_view handler_name,
                       benchmark::State& state) {
  const Config config;
  FakeKvServer server(config);

  CodeConfig code_config;
  code_config.js = code;
  code_config.udf_handler_name = handler_name;

  // If the code is being padded with extra bytes then add a comment at the end
  // and fill it with extra zeroes.
  const int extra_padding_bytes = state.range(1);
  if (extra_padding_bytes > 0) {
    std::string padding = " // ";
    padding += std::string('0', extra_padding_bytes);
    code_config.js += padding;
  }

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  const int number_of_loads = state.range(0);
  for (auto _ : state) {
    for (int i = 0; i < number_of_loads; ++i) {
      server.SetCodeObject(code_config);
    }
  }
  state.SetItemsProcessed(number_of_loads);
  state.SetBytesProcessed(number_of_loads * code_config.js.length());
}

void ExecuteCodeBenchmark(std::string_view code, std::string_view handler_name,
                          benchmark::State& state) {
  const int number_of_calls = state.range(0);
  const Config config;
  FakeKvServer server(config);

  CodeConfig code_config;
  code_config.js = code;
  code_config.udf_handler_name = handler_name;
  server.SetCodeObject(code_config);

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      benchmark::DoNotOptimize(server.ExecuteCode({}));
    }
  }
  state.SetItemsProcessed(number_of_calls);
}

// This C++ callback function is called in the benchmark below:
static void HelloWorldCallback(FunctionBindingIoProto& io) {
  io.set_output_string("I am a callback");
}

void BM_ExecuteHelloWorldCallback(benchmark::State& state) {
  const int number_of_calls = state.range(0);
  Config config;
  {
    auto function_object = std::make_unique<FunctionBindingObjectV2>();
    function_object->function_name = "callback";
    function_object->function = HelloWorldCallback;
    config.RegisterFunctionBinding(std::move(function_object));
  }
  FakeKvServer server(config);

  CodeConfig code_config{
      .js = "hello = () => 'Hello world! ' + callback();",
      .udf_handler_name = "hello",
  };
  server.SetCodeObject(code_config);

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    for (int i = 0; i < number_of_calls; ++i) {
      benchmark::DoNotOptimize(server.ExecuteCode({}));
    }
  }
  state.SetItemsProcessed(number_of_calls);
}

void BM_LoadHelloWorld(benchmark::State& state) {
  LoadCodeBenchmark(kCodeHelloWorld, kHandlerNameHelloWorld, state);
}

void BM_ExecuteHelloWorld(benchmark::State& state) {
  ExecuteCodeBenchmark(kCodeHelloWorld, kHandlerNameHelloWorld, state);
}

void BM_ExecutePrimeSieve(benchmark::State& state) {
  ExecuteCodeBenchmark(kCodePrimeSieve, kHandlerNamePrimeSieve, state);
}

}  // namespace

// Register the function as a benchmark
BENCHMARK(BM_LoadHelloWorld)
    ->ArgsProduct({
        {1, 10, 100},         // Run this many loads of the code.
        {0, 128, 512, 1024},  // Pad with this many extra bytes.
    });
BENCHMARK(BM_ExecuteHelloWorld)->RangeMultiplier(10)->Range(1, 100);
BENCHMARK(BM_ExecuteHelloWorldCallback)->RangeMultiplier(10)->Range(1, 100);
BENCHMARK(BM_ExecutePrimeSieve)->RangeMultiplier(10)->Range(1, 100);

// Run the benchmark
BENCHMARK_MAIN();
