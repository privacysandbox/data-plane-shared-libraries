// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include <benchmark/benchmark.h>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/roma/wasm/testing_utils.h"

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::sandbox::roma_service::RomaService;

namespace {
void BM_PrimeSieveUdfJs(::benchmark::State& state) {
  Config<> config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  CHECK_OK(roma_service.Init());
  {
    absl::Notification done;
    CHECK_OK(
        roma_service.LoadCodeObj(std::make_unique<CodeObject>(CodeObject{
                                     .id = "foo",
                                     .version_string = "v1",
                                     .js = absl::Substitute(R"(
  // Create a boolean array of size n+1
  buffer = new Array($0 + 1).fill(true);

  function sieve() {
    // Set first two values to false
    buffer[0] = false;
    buffer[1] = false;
    // Loop through the elements
    for (let i = 2; i <= Math.sqrt($0); i++) {
       if (buffer[i]) {
          for (let j = i * i; j <= $0; j += i) {
             buffer[j] = false;
          }
       }
    }
    for (let i = $0; i >= 0; i--) {
       if (buffer[i]) {
          return i
       }
    }
    return -1;
  }
)",
                                                            state.range(0)),
                                 }),
                                 [&done](auto response) {
                                   CHECK_OK(response);
                                   done.Notify();
                                 }));
    done.WaitForNotification();
  }
  const InvocationStrRequest<> request{
      .id = "foo",
      .version_string = "v1",
      .handler_name = "sieve",
  };
  {
    absl::Notification done;
    CHECK_OK(roma_service.Execute(
        std::make_unique<InvocationStrRequest<>>(request),
        [&done](auto response) {
          CHECK_OK(response);
          int largest_prime;
          CHECK(absl::SimpleAtoi(response->resp, &largest_prime));
          CHECK_GT(largest_prime, 0);
          done.Notify();
        }));
    done.WaitForNotification();
  }
  for (auto _ : state) {
    absl::Notification done;
    CHECK_OK(
        roma_service.Execute(std::make_unique<InvocationStrRequest<>>(request),
                             [&done](auto response) {
                               CHECK_OK(response);
                               done.Notify();
                             }));
    done.WaitForNotification();
  }
  CHECK_OK(roma_service.Stop());
}

void BM_PrimeSieveUdfWasm(::benchmark::State& state) {
  Config<> config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  CHECK_OK(roma_service.Init());
  {
    absl::Notification done;
    CHECK_OK(roma_service.LoadCodeObj(
        std::make_unique<CodeObject>(CodeObject{
            .id = "foo",
            .version_string = "v1",
            .js = absl::StrCat(
                google::scp::roma::wasm::testing::WasmTestingUtils::
                    LoadJsWithWasmFile(
                        "./src/roma/testing/cpp_wasm_prime_sieve_n_example/"
                        "cpp_wasm_prime_sieve_n_example_generated.js"),
                R"(
  async function sieve(n) {
    const module = await getModule();
    return module.PrimeSieveClass.PrimeSieve(n);
  }
)"),
        }),
        [&done](auto response) {
          CHECK_OK(response);
          done.Notify();
        }));
    done.WaitForNotification();
  }
  const InvocationStrRequest<> request{
      .id = "foo",
      .version_string = "v1",
      .handler_name = "sieve",
      .input = {absl::StrCat(state.range(0))},
  };
  {
    absl::Notification done;
    CHECK_OK(roma_service.Execute(
        std::make_unique<InvocationStrRequest<>>(request),
        [&done](auto response) {
          CHECK_OK(response);
          int largest_prime;
          CHECK(absl::SimpleAtoi(response->resp, &largest_prime));
          CHECK_GT(largest_prime, 0);
          done.Notify();
        }));
    done.WaitForNotification();
  }
  for (auto _ : state) {
    absl::Notification done;
    CHECK_OK(
        roma_service.Execute(std::make_unique<InvocationStrRequest<>>(request),
                             [&done](auto response) {
                               CHECK_OK(response);
                               done.Notify();
                             }));
    done.WaitForNotification();
  }
  CHECK_OK(roma_service.Stop());
}
}  // namespace

BENCHMARK(BM_PrimeSieveUdfJs)
    ->Arg(100'000)
    ->Arg(500'000)
    ->Arg(1'000'000)
    ->Arg(5'000'000)
    ->Arg(10'000'000);

BENCHMARK(BM_PrimeSieveUdfWasm)
    ->Arg(100'000)
    ->Arg(500'000)
    ->Arg(1'000'000)
    ->Arg(5'000'000)
    ->Arg(10'000'000);

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
