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
 * //src/roma/benchmark:proto_serde_benchmark \
 * --test_output=all 2>&1 | grep -Ev "sandbox.cc|monitor_base.cc|sandbox2.cc"
 */

#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/benchmark/test_code.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_js_engine.h"

#include "test_service_romav8_app_service.h"

namespace {

using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using ::privacysandbox::test_server::TestService;

constexpr auto kTimeout = absl::Seconds(10);

TestService<> CreateAppService() {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = TestService<>::Create(std::move(config));
  CHECK_OK(app_svc);
  return std::move(*app_svc);
}

void LoadCodeObj(TestService<>& app_svc, std::string_view code) {
  absl::Notification register_finished;
  absl::Status register_status;
  CHECK_OK(app_svc.Register(register_finished, register_status, code));
  register_finished.WaitForNotificationWithTimeout(kTimeout);
  CHECK_OK(register_status);
}

void RunRomaJsBenchmark(benchmark::State& state, std::string_view code,
                        std::string_view handler) {
  TestService<> app_svc = CreateAppService();
  LoadCodeObj(app_svc, code);

  absl::Duration timing_sum = absl::Milliseconds(0.0);
  std::string result;
  for (auto _ : state) {
    absl::Notification execute_finished;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "1",
            .handler_name = std::string(handler),
        });
    CHECK_OK(app_svc.GetRomaService()->Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK(resp.ok());
          result = std::move(resp->resp);
          execute_finished.Notify();
        }));

    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));

    state.PauseTiming();
    absl::Duration time;
    CHECK(absl::ParseDuration(absl::StrCat(result, "ms"), &time));
    timing_sum += time;
    state.ResumeTiming();
  }

  absl::Duration avg_time = timing_sum / state.iterations();
  state.SetLabel(absl::StrCat("Average Time: ", avg_time));
}

void RunJsEngineBenchmark(benchmark::State& state, std::string_view code,
                          std::string_view handler) {
  TestService<> app_svc = CreateAppService();
  static constexpr bool skip_v8_cleanup = true;
  V8JsEngine engine(nullptr, skip_v8_cleanup);
  engine.OneTimeSetup();
  engine.Run();

  std::string js_code = absl::StrCat(app_svc.GetRomaJsCode(), code);

  auto resp = engine.CompileAndRunJs(js_code, "" /*function_name*/,
                                     {} /*input*/, {} /*metadata*/);
  CHECK_OK(resp);

  absl::Duration timing_sum = absl::Milliseconds(0.0);
  for (auto _ : state) {
    auto execution_response =
        engine.CompileAndRunJs(js_code, handler, {} /*input*/, {} /*metadata*/,
                               resp->compilation_context);
    CHECK_OK(execution_response);

    state.PauseTiming();
    absl::Duration time;
    CHECK(absl::ParseDuration(
        absl::StrCat(execution_response->execution_response.response, "ms"),
        &time));
    timing_sum += time;
    state.ResumeTiming();
  }

  absl::Duration avg_time = timing_sum / state.iterations();
  state.SetLabel(absl::StrCat("Average Time: ", avg_time));

  engine.Stop();
}

void SerializeProtobufBenchmark(benchmark::State& state) {
  ::privacy_sandbox::server_common::TestMethodRequest req;
  req.set_input("foobar");
  for (auto _ : state) {
    benchmark::DoNotOptimize(req.SerializeAsString());
  }
}

void DeserializeProtobufBenchmark(benchmark::State& state) {
  ::privacy_sandbox::server_common::TestMethodRequest inputReq;
  inputReq.set_input("foobar");
  std::string serializedInput = inputReq.SerializeAsString();

  ::privacy_sandbox::server_common::TestMethodRequest outputReq;
  for (auto _ : state) {
    benchmark::DoNotOptimize(outputReq.ParseFromString(serializedInput));
  }
}

void BM_RomaJsSerializeProtobuf(benchmark::State& state) {
  RunRomaJsBenchmark(
      state, google::scp::roma::benchmark::kCodeSerializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameSerializeProtobuf);
}

void BM_RomaJsDeserializeProtobuf(benchmark::State& state) {
  RunRomaJsBenchmark(
      state, google::scp::roma::benchmark::kCodeDeserializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameDeserializeProtobuf);
}

void BM_CppSerializeProtobuf(benchmark::State& state) {
  SerializeProtobufBenchmark(state);
}

void BM_CppDeserializeProtobuf(benchmark::State& state) {
  DeserializeProtobufBenchmark(state);
}

void BM_JsEngineSerializeProtobuf(benchmark::State& state) {
  RunJsEngineBenchmark(
      state, google::scp::roma::benchmark::kCodeDeserializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameDeserializeProtobuf);
}

void BM_JsEngineDeserializeProtobuf(benchmark::State& state) {
  RunJsEngineBenchmark(
      state, google::scp::roma::benchmark::kCodeDeserializeProtobuf,
      google::scp::roma::benchmark::kHandlerNameDeserializeProtobuf);
}

BENCHMARK(BM_RomaJsSerializeProtobuf)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_RomaJsDeserializeProtobuf)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_CppSerializeProtobuf)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_CppDeserializeProtobuf)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_JsEngineSerializeProtobuf)->Unit(benchmark::kMicrosecond);
BENCHMARK(BM_JsEngineDeserializeProtobuf)->Unit(benchmark::kMicrosecond);

}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
