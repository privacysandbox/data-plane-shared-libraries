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
 * //src/roma/benchmark:logging_benchmark \
 * --test_output=all 2>&1 | fgrep -v sandbox2.cc
 */

#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "src/roma/benchmark/test_code.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace {

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::DefaultMetadata;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::benchmark::kCodeHelloWorld;
using google::scp::roma::benchmark::kCodeJetstreamCryptoAes;
using google::scp::roma::benchmark::kCodeJetstreamDeltaBlue;
using google::scp::roma::benchmark::kCodeJetstreamNavierStokes;
using google::scp::roma::benchmark::kCodeJetstreamSplay;
using google::scp::roma::benchmark::kCodeJetstreamUniPoker;
using google::scp::roma::benchmark::kCodePrimeSieve;
using google::scp::roma::benchmark::kHandlerNameHelloWorld;
using google::scp::roma::benchmark::kHandlerNameJetstreamCryptoAes;
using google::scp::roma::benchmark::kHandlerNameJetstreamDeltaBlue;
using google::scp::roma::benchmark::kHandlerNameJetstreamNavierStokes;
using google::scp::roma::benchmark::kHandlerNameJetstreamSplay;
using google::scp::roma::benchmark::kHandlerNameJetstreamUniPoker;
using google::scp::roma::benchmark::kHandlerNamePrimeSieve;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr auto kTimeout = absl::Seconds(10);

std::vector<std::vector<std::string>> CreateOptimizerCombinations();
const std::vector<std::string> kOptimizerOpts = {
    "--turbofan",
    "--maglev",
    "--turboshaft",
};
const std::vector<std::vector<std::string>> kOptimizerCombos =
    CreateOptimizerCombinations();

std::unique_ptr<RomaService<>> roma_service;

void DoSetup(typename RomaService<>::Config config) {
  roma_service.reset(new RomaService<>(std::move(config)));
  CHECK_OK(roma_service->Init());
}

void DoTeardown(const benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
  roma_service.reset();
}

void LoadCodeObj(std::string_view code) {
  absl::Notification load_finished;

  absl::Status load_status = roma_service->LoadCodeObj(
      std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = std::string(code),
      }),
      [&load_finished](const absl::StatusOr<ResponseObject>& resp) {
        CHECK(resp.ok());
        load_finished.Notify();
      });

  CHECK(load_status.ok()) << load_status;
  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));
}

void LoadCodeBenchmark(benchmark::State& state, std::string_view code) {
  for (auto _ : state) {
    LoadCodeObj(code);
  }

  std::string label = absl::StrJoin(kOptimizerCombos[state.range(0)], " ");
  state.SetLabel(label.empty() ? "default" : label);
}

void ExecuteCodeBenchmark(benchmark::State& state, std::string_view code,
                          std::string_view handler) {
  LoadCodeObj(code);
  for (auto _ : state) {
    absl::Notification execute_finished;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = std::string(handler),
        });
    auto execution_status = roma_service->Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK(resp.ok());
          execute_finished.Notify();
        });

    CHECK(execution_status.ok()) << execution_status;
    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  }

  std::string label = absl::StrJoin(kOptimizerCombos[state.range(0)], " ");
  state.SetLabel(label.empty() ? "default" : label);
}

void SetupWithV8Flags(const benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;
  config.SetV8Flags(kOptimizerCombos[state.range(0)]);

  DoSetup(std::move(config));
}

void CreateOptimizerCombinationsHelper(
    const std::vector<std::string>& curr_combination, int index,
    std::vector<std::vector<std::string>>& combinations) {
  if (index == kOptimizerOpts.size()) {
    if (curr_combination.empty()) {
      combinations.push_back({"--no-turbofan"});
    } else {
      combinations.push_back(curr_combination);
    }
    return;
  }

  CreateOptimizerCombinationsHelper(curr_combination, index + 1, combinations);
  std::vector<std::string> new_combination = curr_combination;
  new_combination.push_back(kOptimizerOpts[index]);
  CreateOptimizerCombinationsHelper(new_combination, index + 1, combinations);
}

std::vector<std::vector<std::string>> CreateOptimizerCombinations() {
  std::vector<std::vector<std::string>> optimizer_combos;
  CreateOptimizerCombinationsHelper({}, 0, optimizer_combos);
  return optimizer_combos;
}

void BM_LoadCodeObjHelloWorld(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeHelloWorld);
}

void BM_LoadCodeObjPrimeSieve(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodePrimeSieve);
}

void BM_LoadCodeObjJetstreamUniPoker(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamUniPoker);
}

void BM_LoadCodeObjJetstreamSplay(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamSplay);
}

void BM_LoadCodeObjJetstreamDeltaBlue(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamDeltaBlue);
}

void BM_LoadCodeObjJetstreamCryptoAes(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamCryptoAes);
}

void BM_LoadCodeObjJetstreamNavierStokes(benchmark::State& state) {
  LoadCodeBenchmark(state, kCodeJetstreamNavierStokes);
}

void BM_ExecuteCodeObjHelloWorld(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeHelloWorld, kHandlerNameHelloWorld);
}

void BM_ExecuteCodeObjPrimeSieve(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodePrimeSieve, kHandlerNamePrimeSieve);
}

void BM_ExecuteCodeObjJetstreamUniPoker(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamUniPoker,
                       kHandlerNameJetstreamUniPoker);
}

void BM_ExecuteCodeObjJetstreamSplay(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamSplay, kHandlerNameJetstreamSplay);
}

void BM_ExecuteCodeObjJetstreamDeltaBlue(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamDeltaBlue,
                       kHandlerNameJetstreamDeltaBlue);
}

void BM_ExecuteCodeObjJetstreamCryptoAes(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamCryptoAes,
                       kHandlerNameJetstreamCryptoAes);
}

void BM_ExecuteCodeObjJetstreamNavierStokes(benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamNavierStokes,
                       kHandlerNameJetstreamNavierStokes);
}

BENCHMARK(BM_LoadCodeObjHelloWorld)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjPrimeSieve)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamUniPoker)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamSplay)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamDeltaBlue)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamCryptoAes)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadCodeObjJetstreamNavierStokes)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjHelloWorld)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjPrimeSieve)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamUniPoker)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamSplay)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamDeltaBlue)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamCryptoAes)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamNavierStokes)
    ->DenseRange(0, kOptimizerCombos.size() - 1)
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);

}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
