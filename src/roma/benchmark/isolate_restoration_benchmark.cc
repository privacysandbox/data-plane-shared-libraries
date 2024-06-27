/*
 * Copyright 2024 Google LLC
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
 * //src/roma/benchmark:isolate_restoration_benchmark \
 * --test_output=all 2>&1 | grep -Ev "sandbox.cc|monitor_base.cc|sandbox2.cc"
 */

#include <fstream>
#include <memory>
#include <regex>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace {

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::DefaultMetadata;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr std::string_view kHandlerName = "Handler";
constexpr absl::Duration kTimeout = absl::Seconds(10);
constexpr int32_t kMinIter = 10'000;
constexpr int32_t kMaxIter = 1'000'000;
constexpr std::string_view kGlobalStructureUdfPath =
    "./src/roma/tools/v8_cli/test_udfs/global_vars/global_structure_10K.js";
constexpr std::string_view kGlobalStringUdfPath =
    "./src/roma/tools/v8_cli/test_udfs/global_vars/global_string_10K.js";

std::unique_ptr<RomaService<>> roma_service;

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

void DoSetup(const ::benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;

  roma_service.reset(new RomaService<>(std::move(config)));
  CHECK_OK(roma_service->Init());
}

std::string GetGlobalVariableUdf(int iter, bool benchmark_structure) {
  std::string_view udf_path =
      benchmark_structure ? kGlobalStructureUdfPath : kGlobalStringUdfPath;
  std::ifstream inputFile((std::string(udf_path)));
  std::string code((std::istreambuf_iterator<char>(inputFile)),
                   (std::istreambuf_iterator<char>()));
  CHECK(!code.empty());
  std::regex pattern(R"(const kDefaultIter = \w+;)");

  return std::regex_replace(code, pattern,
                            absl::StrCat("const kDefaultIter = ", iter, ";"));
}

void LoadBenchmark(::benchmark::State& state, bool benchmark_structure) {
  auto iter = state.range(0);
  std::string code = GetGlobalVariableUdf(iter, benchmark_structure);

  for (auto _ : state) {
    LoadCodeObj(code);
  }
}

void BM_LoadGlobalStructure(::benchmark::State& state) {
  bool benchmark_structure = true;
  LoadBenchmark(state, benchmark_structure);
}

void BM_LoadGlobalString(::benchmark::State& state) {
  bool benchmark_structure = false;
  LoadBenchmark(state, benchmark_structure);
}

void ExecuteBenchmark(::benchmark::State& state, bool benchmark_structure) {
  auto iter = state.range(0);
  std::string code = GetGlobalVariableUdf(iter, benchmark_structure);
  LoadCodeObj(code);

  for (auto _ : state) {
    absl::Notification execute_finished;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = std::string(kHandlerName),
        });
    CHECK_OK(roma_service->Execute(std::move(execution_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     CHECK_OK(resp);
                                     execute_finished.Notify();
                                   }));

    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  }
}

void BM_ExecuteGlobalStructure(::benchmark::State& state) {
  bool benchmark_structure = true;
  ExecuteBenchmark(state, benchmark_structure);
}

void BM_ExecuteGlobalString(::benchmark::State& state) {
  bool benchmark_structure = false;
  ExecuteBenchmark(state, benchmark_structure);
}

BENCHMARK(BM_LoadGlobalStructure)
    ->Range(kMinIter, kMaxIter)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
BENCHMARK(BM_LoadGlobalString)
    ->Range(kMinIter, kMaxIter)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteGlobalStructure)
    ->Range(kMinIter, kMaxIter)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
BENCHMARK(BM_ExecuteGlobalString)
    ->Range(kMinIter, kMaxIter)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown);
}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
