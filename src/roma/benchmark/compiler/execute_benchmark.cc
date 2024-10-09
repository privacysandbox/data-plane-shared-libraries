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
 */

#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "src/roma/benchmark/compiler/compiler_utils.h"
#include "src/roma/benchmark/test_code.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::benchmark::compiler {

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

std::unique_ptr<RomaService<>> roma_service;

void DoSetup(typename RomaService<>::Config config) {
  roma_service.reset(new RomaService<>(std::move(config)));
  CHECK_OK(roma_service->Init());
}

void DoTeardown(const ::benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
  roma_service.reset();
}

void LoadCodeObj(std::string_view code) {
  absl::Notification load_finished;

  CHECK_OK(roma_service->LoadCodeObj(
      std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = std::string(code),
      }),
      [&load_finished](const absl::StatusOr<ResponseObject>& resp) {
        CHECK_OK(resp);
        load_finished.Notify();
      }));

  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));
}

void ExecuteCodeBenchmark(::benchmark::State& state, std::string_view code,
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
    CHECK_OK(roma_service->Execute(std::move(execution_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     CHECK_OK(resp);
                                     execute_finished.Notify();
                                   }));

    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  }

  std::string label;
  if (state.range(0) == kOptimizerCombos.size()) {
    label = "--no-turbofan";
  } else {
    if (kOptimizerCombos[state.range(0)].enable_turbofan) {
      label += "--turbofan ";
    }
    if (kOptimizerCombos[state.range(0)].enable_maglev) {
      label += "--maglev ";
    }
    if (kOptimizerCombos[state.range(0)].enable_turboshaft) {
      label += "--turboshaft ";
    }
  }
  state.SetLabel(label.empty() ? "default" : label);
}

void SetupWithV8Flags(const ::benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;
  // Benchmarks run with range [0, kOptimizerCombos.size()] to be used as
  // indices into kOptimizerCombos. kOptimizerCombos.size() is an invalid index
  // used as a special case to run --no-turbofan.
  if (state.range(0) == kOptimizerCombos.size()) {
    std::vector<std::string>& v8_flags = config.SetV8Flags();
    v8_flags.push_back("--no-turbofan");
  } else {
    config.ConfigureV8Compilers(kOptimizerCombos[state.range(0)]);
  }

  DoSetup(std::move(config));
}

void BM_ExecuteCodeObjHelloWorld(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeHelloWorld, kHandlerNameHelloWorld);
}

void BM_ExecuteCodeObjPrimeSieve(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodePrimeSieve, kHandlerNamePrimeSieve);
}

void BM_ExecuteCodeObjJetstreamUniPoker(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamUniPoker,
                       kHandlerNameJetstreamUniPoker);
}

void BM_ExecuteCodeObjJetstreamSplay(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamSplay, kHandlerNameJetstreamSplay);
}

void BM_ExecuteCodeObjJetstreamDeltaBlue(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamDeltaBlue,
                       kHandlerNameJetstreamDeltaBlue);
}

void BM_ExecuteCodeObjJetstreamCryptoAes(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamCryptoAes,
                       kHandlerNameJetstreamCryptoAes);
}

void BM_ExecuteCodeObjJetstreamNavierStokes(::benchmark::State& state) {
  ExecuteCodeBenchmark(state, kCodeJetstreamNavierStokes,
                       kHandlerNameJetstreamNavierStokes);
}

BENCHMARK(BM_ExecuteCodeObjHelloWorld)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjPrimeSieve)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamUniPoker)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamSplay)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamDeltaBlue)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamCryptoAes)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteCodeObjJetstreamNavierStokes)
    ->DenseRange(0, kOptimizerCombos.size())
    ->Setup(SetupWithV8Flags)
    ->Teardown(DoTeardown);

}  // namespace google::scp::roma::benchmark::compiler

// Run the benchmark
BENCHMARK_MAIN();
