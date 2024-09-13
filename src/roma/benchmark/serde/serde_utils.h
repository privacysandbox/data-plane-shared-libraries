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
 */

#ifndef ROMA_BENCHMARK_PROTO_PROTO_SERDE_UTILS_H_
#define ROMA_BENCHMARK_PROTO_PROTO_SERDE_UTILS_H_

#include <fcntl.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <benchmark/benchmark.h>
#include <google/protobuf/text_format.h>
#include <nlohmann/json.hpp>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/benchmark/serde/benchmark_service.pb.h"
#include "src/roma/benchmark/test_code.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_js_engine.h"

#include "benchmark_service_romav8_app_service.h"

namespace google::scp::roma::benchmark::proto {

using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using privacy_sandbox::benchmark::BenchmarkRequest;
using privacy_sandbox::benchmark::V8BenchmarkService;

constexpr auto kTimeout = absl::Seconds(10);
constexpr std::string_view kCodeVersion = "v1";

constexpr std::string_view kSmallProtoPath =
    "./src/roma/benchmark/serde/benchmark_request_small.txtpb";
constexpr std::string_view kMediumProtoPath =
    "./src/roma/benchmark/serde/benchmark_request_medium.txtpb";
constexpr std::string_view kLargeProtoPath =
    "./src/roma/benchmark/serde/benchmark_request_large.txtpb";
constexpr std::string_view kSmallJsonPath =
    "./src/roma/benchmark/serde/benchmark_request_small.json";
constexpr std::string_view kMediumJsonPath =
    "./src/roma/benchmark/serde/benchmark_request_medium.json";
constexpr std::string_view kLargeJsonPath =
    "./src/roma/benchmark/serde/benchmark_request_large.json";

BenchmarkRequest GetProtoFromPath(std::string_view path) {
  BenchmarkRequest req;
  int fd = open(path.data(), O_RDONLY);
  CHECK_GE(fd, 0);
  google::protobuf::io::FileInputStream file_input_stream(fd);
  file_input_stream.SetCloseOnDelete(true);
  CHECK(google::protobuf::TextFormat::Parse(&file_input_stream, &req));
  return req;
}

V8BenchmarkService<> CreateAppService() {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8BenchmarkService<>::Create(std::move(config));
  CHECK_OK(app_svc);
  return std::move(*app_svc);
}

void LoadCodeObj(V8BenchmarkService<>& app_svc, std::string_view code) {
  absl::Notification register_finished;
  absl::Status register_status;
  CHECK_OK(
      app_svc.Register(code, kCodeVersion, register_finished, register_status));
  register_finished.WaitForNotificationWithTimeout(kTimeout);
  CHECK_OK(register_status);
}

void RunRomaJsBenchmark(::benchmark::State& state, std::string_view code,
                        std::string_view handler, std::string_view data_path) {
  V8BenchmarkService<> app_svc = CreateAppService();
  LoadCodeObj(app_svc, code);

  std::ifstream input_file(data_path.data());
  nlohmann::json json = nlohmann::json::parse(input_file);
  std::string input = json.dump();

  absl::Duration timing_sum = absl::Milliseconds(0.0);
  std::string result;
  for (auto _ : state) {
    absl::Notification execute_finished;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = std::string(kCodeVersion),
            .handler_name = std::string(handler),
            .input = {input},
        });
    CHECK_OK(app_svc.GetRomaService()->Execute(
        std::move(execution_obj), [&](absl::StatusOr<ResponseObject> resp) {
          CHECK_OK(resp);
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
}

}  // namespace google::scp::roma::benchmark::proto

#endif  // ROMA_BENCHMARK_PROTO_PROTO_SERDE_UTILS_H_
