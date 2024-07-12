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

#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace {

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::DefaultMetadata;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::FunctionBindingPayload;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr auto kTimeout = absl::Seconds(10);

void LoggingFunction(absl::LogSeverity severity, DefaultMetadata metadata,
                     std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
}

std::unique_ptr<RomaService<>> roma_service;

void DoSetup(const benchmark::State& state) {
  Config config;
  config.number_of_workers = 2;
  config.SetLoggingFunction(LoggingFunction);
  roma_service = std::make_unique<RomaService<>>(std::move(config));
  CHECK_OK(roma_service->Init());

  absl::Notification load_finished;
  {
    const std::string js = R"(
  function Handler(length, iters) {
    const log_msg = "x".repeat(length);
    for (let n = 0; n < iters; n++) {
        console.log(log_msg);
    }
  })";

    auto code_obj = std::make_unique<CodeObject>(CodeObject{
        .id = "foo",
        .version_string = "v1",
        .js = js,
    });
    CHECK_OK(roma_service->LoadCodeObj(
        std::move(code_obj), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
          load_finished.Notify();
        }));
  }
  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));
}

void DoTeardown(const benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
}

void NumLogsByLengthBenchmark(int length, int iters, benchmark::State& state) {
  absl::Notification execute_finished;
  {
    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {absl::StrCat(length), absl::StrCat(iters)},
        });
    CHECK_OK(roma_service->Execute(std::move(execution_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     CHECK_OK(resp);
                                     execute_finished.Notify();
                                   }));
  }
  CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
  state.SetLabel(absl::StrCat("Log size: ", length, ", Num Iters: ", iters));
}

void BM_RomaLogging(benchmark::State& state) {
  for (auto _ : state) {
    const auto iters = state.range(0);
    const auto len = state.range(1);
    NumLogsByLengthBenchmark(len, iters, state);
  }
}

BENCHMARK(BM_RomaLogging)
    ->Setup(DoSetup)
    ->Teardown(DoTeardown)
    ->ArgsProduct({
        // Number of log iterations per invocation
        benchmark::CreateRange(1, 10'000, /*multi=*/10),
        // Size of each log message
        benchmark::CreateRange(1, 100'000, /*multi=*/10),
    });

}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
