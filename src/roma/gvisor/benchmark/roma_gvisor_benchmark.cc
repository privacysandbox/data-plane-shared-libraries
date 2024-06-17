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
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

#include <grpcpp/grpcpp.h>

#include <benchmark/benchmark.h>
#include <google/protobuf/message_lite.h>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"
#include "src/roma/gvisor/interface/roma_service.h"
#include "src/roma/gvisor/udf/roma_binary.pb.h"

namespace {
using google::scp::roma::FunctionBindingObjectV2;
using privacy_sandbox::server_common::gvisor::BinaryRequest;
using privacy_sandbox::server_common::gvisor::BinaryResponse;
using privacy_sandbox::server_common::gvisor::Mode;
using privacy_sandbox::server_common::gvisor::RomaService;

constexpr int kPrimeCount = 9592;
constexpr std::string_view kGoLangBinaryPath = "/server/bin/sample_go_udf";
constexpr std::string_view kCPlusPlusBinaryPath = "/server/bin/sample_udf";
constexpr std::string_view kCPlusPlusNewBinaryPath = "/server/bin/new_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";

enum class Language {
  kCPlusPlus = 0,
  kGoLang = 1,
};

BinaryResponse SendRequestAndGetResponse(RomaService<>* roma_service,
                                         BinaryRequest::Function func_type,
                                         std::string_view code_token) {
  // Data we are sending to the server.
  BinaryRequest bin_request;
  bin_request.set_function(func_type);
  std::unique_ptr<BinaryResponse> bin_response =
      std::make_unique<BinaryResponse>();
  absl::StatusOr<std::unique_ptr<::google::protobuf::MessageLite>> response =
      std::move(bin_response);

  absl::Notification notif;
  CHECK_OK(roma_service->ExecuteBinary(code_token, bin_request,
                                       /*metadata=*/{}, response, notif));
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(response);
  return std::move(*static_cast<BinaryResponse*>(response->get()));
}

std::string LoadCode(RomaService<>* roma_service, std::string_view file_path) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service->LoadBinary(file_path, notif, notif_status);
  CHECK_OK(code_id);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  CHECK_OK(notif_status);
  return *std::move(code_id);
}

std::unique_ptr<RomaService<>> GetRomaService(Mode mode, int num_workers) {
  privacy_sandbox::server_common::gvisor::Config config = {
      .num_workers = num_workers,
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example", [](auto&) {}}},
  };
  absl::StatusOr<std::unique_ptr<RomaService<>>> roma_interface =
      RomaService<>::Create(config, mode);
  CHECK_OK(roma_interface);
  return *std::move(roma_interface);
}

void VerifyResponse(
    BinaryResponse bin_response, std::string_view expected_response,
    BinaryRequest::Function func = BinaryRequest::FUNCTION_HELLO_WORLD) {
  switch (func) {
    case BinaryRequest::FUNCTION_HELLO_WORLD:
      CHECK(absl::EqualsIgnoreCase(bin_response.greeting(), expected_response))
          << "Actual response: " << bin_response.greeting()
          << "\tExpected response: " << expected_response;
      break;
    case BinaryRequest::FUNCTION_PRIME_SIEVE:
      CHECK_EQ(bin_response.prime_number_size(), kPrimeCount)
          << "Expected " << kPrimeCount << " upto 100,000";
      break;
    default:
      LOG(ERROR) << "Unexpected input";
      abort();
  }
}

std::string_view GetFilePathFromLanguage(Language lang) {
  switch (lang) {
    case Language::kCPlusPlus:
      return kCPlusPlusBinaryPath;
    case Language::kGoLang:
      return kGoLangBinaryPath;
    default:
      return "";
  }
}

std::string GetModeStr(Mode mode) {
  switch (mode) {
    case Mode::kModeGvisor:
      return "mode:gVisor";
    case Mode::kModeLocal:
      return "mode:Local";
    default:
      return "mode:Unknown";
  }
}

std::string GetLanguageStr(Language lang) {
  switch (lang) {
    case Language::kCPlusPlus:
      return "language:C++";
    case Language::kGoLang:
      return "language:Go";
    default:
      return "mode:Unknown";
  }
}

std::string GetFunctionTypeStr(BinaryRequest::Function func_type) {
  switch (func_type) {
    case BinaryRequest::FUNCTION_HELLO_WORLD:
      return R"(udf:"Hello World")";
    case BinaryRequest::FUNCTION_PRIME_SIEVE:
      return R"(udf:"Prime Sieve")";
    case BinaryRequest::FUNCTION_CALLBACK:
      return R"(udf:"Callback hook")";
    case BinaryRequest::FUNCTION_TEN_CALLBACK_INVOCATIONS:
      return R"(udf:"Ten callback invocations")";
    default:
      return "udf:Unknown";
  }
}
}  // namespace

void BM_LoadBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaService<>> roma_service =
      GetRomaService(mode, /*num_workers=*/1);
  BinaryRequest::Function func_type = BinaryRequest::FUNCTION_HELLO_WORLD;

  auto bin_response = SendRequestAndGetResponse(
      roma_service.get(), func_type,
      LoadCode(roma_service.get(), kCPlusPlusBinaryPath));
  VerifyResponse(bin_response, kFirstUdfOutput);

  std::string code_token;
  for (auto _ : state) {
    code_token = LoadCode(roma_service.get(), kCPlusPlusNewBinaryPath);
  }
  bin_response =
      SendRequestAndGetResponse(roma_service.get(), func_type, code_token);
  VerifyResponse(bin_response, kNewUdfOutput);
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_LoadTwoBinariesAndExecuteFirstBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaService<>> roma_service =
      GetRomaService(mode, /*num_workers=*/2);

  std::string first_code_token =
      LoadCode(roma_service.get(), kCPlusPlusBinaryPath);
  std::string second_code_token =
      LoadCode(roma_service.get(), kCPlusPlusNewBinaryPath);

  BinaryRequest::Function func_type = BinaryRequest::FUNCTION_HELLO_WORLD;
  VerifyResponse(SendRequestAndGetResponse(roma_service.get(), func_type,
                                           first_code_token),
                 kFirstUdfOutput);

  VerifyResponse(SendRequestAndGetResponse(roma_service.get(), func_type,
                                           second_code_token),
                 kNewUdfOutput);

  for (auto _ : state) {
    auto response = SendRequestAndGetResponse(roma_service.get(), func_type,
                                              first_code_token);
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_LoadTwoBinariesAndExecuteSecondBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaService<>> roma_service =
      GetRomaService(mode, /*num_workers=*/2);

  std::string first_code_token =
      LoadCode(roma_service.get(), kCPlusPlusBinaryPath);
  std::string second_code_token =
      LoadCode(roma_service.get(), kCPlusPlusNewBinaryPath);

  BinaryRequest::Function func_type = BinaryRequest::FUNCTION_HELLO_WORLD;
  VerifyResponse(SendRequestAndGetResponse(roma_service.get(), func_type,
                                           first_code_token),
                 kFirstUdfOutput);

  VerifyResponse(SendRequestAndGetResponse(roma_service.get(), func_type,
                                           second_code_token),
                 kNewUdfOutput);

  for (auto _ : state) {
    auto response = SendRequestAndGetResponse(roma_service.get(), func_type,
                                              second_code_token);
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ExecuteBinaryAsyncUnaryGrpc(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaService<>> roma_service =
      GetRomaService(mode, /*num_workers=*/state.range(2));

  std::string code_token = LoadCode(roma_service.get(), kCPlusPlusBinaryPath);

  BinaryRequest::Function func_type =
      static_cast<BinaryRequest::Function>(state.range(1));
  auto response =
      SendRequestAndGetResponse(roma_service.get(), func_type, code_token);

  for (auto s : state) {
    response =
        SendRequestAndGetResponse(roma_service.get(), func_type, code_token);
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_GvisorCompareCPlusPlusAndGoLangBinary(benchmark::State& state) {
  Language lang = static_cast<Language>(state.range(0));
  std::unique_ptr<RomaService<>> roma_service =
      GetRomaService(Mode::kModeGvisor, /*num_workers=*/2);

  std::string code_token =
      LoadCode(roma_service.get(), GetFilePathFromLanguage(lang));

  BinaryRequest::Function func_type =
      static_cast<BinaryRequest::Function>(state.range(1));
  VerifyResponse(
      SendRequestAndGetResponse(roma_service.get(), func_type, code_token),
      lang == Language::kCPlusPlus ? kFirstUdfOutput : kGoBinaryOutput,
      func_type);

  for (auto _ : state) {
    auto response =
        SendRequestAndGetResponse(roma_service.get(), func_type, code_token);
  }
  state.SetLabel(absl::StrJoin(
      {GetFunctionTypeStr(func_type), GetLanguageStr(lang)}, ", "));
}

BENCHMARK(BM_LoadBinary)
    ->Unit(benchmark::kMillisecond)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
    })
    ->ArgNames({"mode"});

BENCHMARK(BM_GvisorCompareCPlusPlusAndGoLangBinary)
    ->Unit(benchmark::kMillisecond)
    ->ArgsProduct({
        {
            (int)Language::kCPlusPlus,
            (int)Language::kGoLang,
        },
        {
            BinaryRequest::FUNCTION_HELLO_WORLD,  // Generic "Hello, world!"
            BinaryRequest::FUNCTION_PRIME_SIEVE,  // Sieve of primes
        },
    })
    ->ArgNames({"mode", "udf"});

BENCHMARK(BM_LoadTwoBinariesAndExecuteFirstBinary)
    ->Unit(benchmark::kMillisecond)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
    })
    ->ArgNames({"mode"});

BENCHMARK(BM_LoadTwoBinariesAndExecuteSecondBinary)
    ->Unit(benchmark::kMillisecond)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
    })
    ->ArgNames({"mode"});

BENCHMARK(BM_ExecuteBinaryAsyncUnaryGrpc)
    ->Unit(benchmark::kMillisecond)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
        {
            BinaryRequest::FUNCTION_HELLO_WORLD,  // Generic "Hello, world!"
            BinaryRequest::FUNCTION_PRIME_SIEVE,  // Sieve of primes
            BinaryRequest::FUNCTION_CALLBACK,     // Generic callback hook
            BinaryRequest::
                FUNCTION_TEN_CALLBACK_INVOCATIONS,  // Ten invocations of
                                                    // generic callback hook
        },
        {
            0, 1, 10, 20, 50, 100, 250  // Number of pre-warmed workers
        },
    })
    ->ArgNames({"mode", "udf", "num_pre_warmed_workers"});

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
