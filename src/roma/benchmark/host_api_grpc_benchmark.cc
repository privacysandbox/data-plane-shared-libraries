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

#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/native_function_grpc_server/proto/callback_service.pb.h"
#include "src/roma/native_function_grpc_server/proto/test_host_service_native_request_handler.h"
#include "src/roma/roma_service/roma_service.h"

namespace {

using google::scp::roma::Config;
using google::scp::roma::DefaultMetadata;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr auto kTimeout = absl::Seconds(10);

std::unique_ptr<RomaService<>> roma_service;

void DoSetup(typename RomaService<>::Config config) {
  roma_service.reset(new RomaService<>(std::move(config)));
  CHECK_OK(roma_service->Init());

  absl::Notification load_finished;
  const std::string js = R"(
        function Handler(input) {
          return TestHostServer.NativeMethod(input);
        })";

  using google::scp::roma::CodeObject;
  CHECK_OK(roma_service->LoadCodeObj(
      std::make_unique<CodeObject>(CodeObject{
          .id = "foo",
          .version_string = "v1",
          .js = js,
      }),
      [&load_finished](const absl::StatusOr<ResponseObject>& resp) {
        CHECK_OK(resp);
        load_finished.Notify();
      }));

  CHECK(load_finished.WaitForNotificationWithTimeout(kTimeout));
}

void SetupDeclarativeApiGrpcServer(const benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;
  config.enable_native_function_grpc_server = true;
  config.RegisterRpcHandler(
      "TestHostServer.NativeMethod",
      privacy_sandbox::test_host_server::NativeMethodHandler<
          DefaultMetadata>());
  DoSetup(std::move(config));
}

void SetupDeclarativeApiNativeFunctionHandler(const benchmark::State& state) {
  typename RomaService<>::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "TestHostServer.NativeMethod",
          .function =
              privacy_sandbox::test_host_server::NativeMethodFunctionBinding<
                  DefaultMetadata>,
      }));
  DoSetup(std::move(config));
}

void StringInStringOutFunction(
    google::scp::roma::FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(wrapper.io_proto.input_string() +
                                     "World. From SERVER");
}

void SetupNonDeclarativeApiNativeFunctionHandler(
    const benchmark::State& state) {
  using google::scp::roma::FunctionBindingObjectV2;
  typename RomaService<>::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "TestHostServer.NativeMethod",
          .function = StringInStringOutFunction,
      }));
  DoSetup(std::move(config));
}

void DoTeardown(const benchmark::State& state) {
  CHECK_OK(roma_service->Stop());
  roma_service.reset();
}

void RunBenchmark(benchmark::State& state, std::string_view input,
                  std::string_view output) {
  using google::scp::roma::InvocationStrRequest;
  std::string result;
  result.reserve(100);
  for (auto _ : state) {
    absl::Notification execute_finished;

    auto execution_obj =
        std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
            .id = "foo",
            .version_string = "v1",
            .handler_name = "Handler",
            .input = {std::string(input)},
        });
    CHECK_OK(roma_service->Execute(std::move(execution_obj),
                                   [&](absl::StatusOr<ResponseObject> resp) {
                                     CHECK_OK(resp);
                                     result = std::move(resp->resp);
                                     execute_finished.Notify();
                                   }));

    CHECK(execute_finished.WaitForNotificationWithTimeout(kTimeout));
    CHECK_EQ(result, output);
  }
}

void BM_NonDeclarativeApiNativeFunctionHandler(benchmark::State& state) {
  constexpr std::string_view input = R"("Hello ")";
  constexpr std::string_view output = R"("Hello World. From SERVER")";
  RunBenchmark(state, input, output);
}

BENCHMARK(BM_NonDeclarativeApiNativeFunctionHandler)
    ->Setup(SetupNonDeclarativeApiNativeFunctionHandler)
    ->Teardown(DoTeardown);

void BM_DeclarativeApiNativeFunctionHandler(benchmark::State& state) {
  constexpr std::string_view input = R"("\n\u0006Hello ")";
  constexpr std::string_view output =
      R"("\n\u001eHello World. From NativeMethod")";
  RunBenchmark(state, input, output);
}

BENCHMARK(BM_DeclarativeApiNativeFunctionHandler)
    ->Setup(SetupDeclarativeApiNativeFunctionHandler)
    ->Teardown(DoTeardown);

void BM_DeclarativeApiGrpcServer(benchmark::State& state) {
  constexpr std::string_view input = R"("\n\u0006Hello ")";
  constexpr std::string_view output =
      R"("\n\u001eHello World. From NativeMethod")";
  RunBenchmark(state, input, output);
}

BENCHMARK(BM_DeclarativeApiGrpcServer)
    ->Setup(SetupDeclarativeApiGrpcServer)
    ->Teardown(DoTeardown);

}  // namespace

// Run the benchmark
BENCHMARK_MAIN();
