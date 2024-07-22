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

#include <algorithm>
#include <limits>
#include <memory>
#include <random>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
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
void BM_SortNumbersListUdfJs(::benchmark::State& state) {
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
  numbers = Array.from({length: $0}, () => Math.random())
  function sort() {
    numbers.sort((a, b) => a - b);
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
      .handler_name = "sort",
  };
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

void BM_SortNumbersListUdfWasm(::benchmark::State& state) {
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
                        "src/roma/testing/cpp_wasm_sort_list_example/"
                        "cpp_wasm_sort_list_example_generated.js"),
                absl::Substitute(R"(
  numbers = Array.from({length: $0}, () => Math.random())
  async function sort() {
    const module = await getModule();
    module.SortListClass.SortList(numbers);
  }
)",
                                 state.range(0))),
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
      .handler_name = "sort",
  };
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

BENCHMARK(BM_SortNumbersListUdfJs)->Arg(10'000)->Arg(100'000)->Arg(1'000'000);
BENCHMARK(BM_SortNumbersListUdfWasm)->Arg(10'000)->Arg(100'000)->Arg(1'000'000);

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
