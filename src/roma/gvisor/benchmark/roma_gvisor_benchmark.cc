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

#include <grpcpp/grpcpp.h>

#include <benchmark/benchmark.h>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"
#include "src/roma/gvisor/interface/roma_gvisor.h"
#include "src/roma/gvisor/interface/roma_interface.h"
#include "src/roma/gvisor/interface/roma_local.h"
#include "src/roma/gvisor/udf/roma_binary.pb.h"

namespace {
using google::scp::roma::FunctionBindingObjectV2;
using privacy_sandbox::server_common::gvisor::BinaryRequest;
using privacy_sandbox::server_common::gvisor::BinaryResponse;
using privacy_sandbox::server_common::gvisor::ExecuteBinaryRequest;
using privacy_sandbox::server_common::gvisor::ExecuteBinaryResponse;
using privacy_sandbox::server_common::gvisor::LoadBinaryRequest;
using privacy_sandbox::server_common::gvisor::LoadBinaryResponse;
using privacy_sandbox::server_common::gvisor::RomaClient;
using privacy_sandbox::server_common::gvisor::RomaGvisor;
using privacy_sandbox::server_common::gvisor::RomaInterface;
using privacy_sandbox::server_common::gvisor::RomaLocal;

constexpr int kPrimeCount = 9592;
constexpr std::string_view kGoLangBinaryPath = "/server/bin/sample_go_udf";
constexpr std::string_view kCPlusPlusBinaryPath = "/server/bin/sample_udf";
constexpr std::string_view kCPlusPlusNewBinaryPath = "/server/bin/new_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";

enum class Mode {
  kModeGvisor = 0,
  kModeLocal = 1,
};

enum class Language {
  kCPlusPlus = 0,
  kGoLang = 1,
};

ExecuteBinaryRequest ConstructExecuteBinaryRequest(
    BinaryRequest::Function func_type) {
  // Data we are sending to the server.
  BinaryRequest bin_request;
  bin_request.set_function(func_type);
  ExecuteBinaryRequest request;
  request.set_serialized_request(bin_request.SerializeAsString());
  return request;
}

std::unique_ptr<RomaInterface> GetRomaInterface(Mode mode, int num_workers) {
  privacy_sandbox::server_common::gvisor::Config config = {
      .num_workers = num_workers,
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example", [](auto&) {}}},
  };
  absl::StatusOr<std::unique_ptr<RomaInterface>> roma_interface;
  if (mode == Mode::kModeGvisor) {
    roma_interface = RomaGvisor::Create(config);
  } else {
    roma_interface = RomaLocal::Create(config);
  }
  CHECK_OK(roma_interface);
  return std::move(*roma_interface);
}

void VerifyResponse(
    absl::StatusOr<ExecuteBinaryResponse> exec_response,
    std::string_view expected_response,
    BinaryRequest::Function func = BinaryRequest::FUNCTION_HELLO_WORLD) {
  CHECK_OK(exec_response);
  BinaryResponse bin_response;
  bin_response.ParseFromString(exec_response->serialized_response());
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

std::string GetFileContent(std::string_view path) {
  const std::ifstream input_stream(path.data(), std::ios_base::binary);
  CHECK(!input_stream.fail()) << "Failed to open file";
  std::stringstream buffer;
  buffer << input_stream.rdbuf();
  return buffer.str();
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
    case BinaryRequest::FUNCTION_CONCURRENT_CALLBACKS:
      return R"(udf:"Concurrent callback invocations")";
    default:
      return "udf:Unknown";
  }
}
}  // namespace

void BM_LoadBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaInterface> roma_interface =
      GetRomaInterface(mode, /*num_workers=*/1);

  auto load_response =
      roma_interface->LoadBinary(GetFileContent(kCPlusPlusBinaryPath));
  CHECK_OK(load_response);

  BinaryRequest::Function func_type = BinaryRequest::FUNCTION_HELLO_WORLD;
  ExecuteBinaryRequest request = ConstructExecuteBinaryRequest(func_type);
  request.set_code_token(load_response->code_token());
  VerifyResponse(roma_interface->ExecuteBinary(request), kFirstUdfOutput);

  std::string code_str = GetFileContent(kCPlusPlusNewBinaryPath);
  for (auto _ : state) {
    load_response = roma_interface->LoadBinary(code_str);
  }
  request.set_code_token(load_response->code_token());
  VerifyResponse(roma_interface->ExecuteBinary(request), kNewUdfOutput);
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_LoadTwoBinariesAndExecuteFirstBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaInterface> roma_interface =
      GetRomaInterface(mode, /*num_workers=*/2);

  auto first_load_response =
      roma_interface->LoadBinary(GetFileContent(kCPlusPlusBinaryPath));
  CHECK_OK(first_load_response);
  auto second_load_response =
      roma_interface->LoadBinary(GetFileContent(kCPlusPlusNewBinaryPath));
  CHECK_OK(second_load_response);

  BinaryRequest::Function func_type = BinaryRequest::FUNCTION_HELLO_WORLD;
  ExecuteBinaryRequest request_for_first_binary =
      ConstructExecuteBinaryRequest(func_type);
  request_for_first_binary.set_code_token(first_load_response->code_token());
  VerifyResponse(roma_interface->ExecuteBinary(request_for_first_binary),
                 kFirstUdfOutput);

  ExecuteBinaryRequest request_for_second_binary =
      ConstructExecuteBinaryRequest(func_type);
  request_for_second_binary.set_code_token(second_load_response->code_token());
  VerifyResponse(roma_interface->ExecuteBinary(request_for_second_binary),
                 kNewUdfOutput);

  for (auto _ : state) {
    CHECK_OK(roma_interface->ExecuteBinary(request_for_first_binary));
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_LoadTwoBinariesAndExecuteSecondBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaInterface> roma_interface =
      GetRomaInterface(mode, /*num_workers=*/2);

  auto first_load_response =
      roma_interface->LoadBinary(GetFileContent(kCPlusPlusBinaryPath));
  CHECK_OK(first_load_response);
  auto second_load_response =
      roma_interface->LoadBinary(GetFileContent(kCPlusPlusNewBinaryPath));
  CHECK_OK(second_load_response);

  BinaryRequest::Function func_type = BinaryRequest::FUNCTION_HELLO_WORLD;
  ExecuteBinaryRequest request_for_first_binary =
      ConstructExecuteBinaryRequest(func_type);
  request_for_first_binary.set_code_token(first_load_response->code_token());
  VerifyResponse(roma_interface->ExecuteBinary(request_for_first_binary),
                 kFirstUdfOutput);

  ExecuteBinaryRequest request_for_second_binary =
      ConstructExecuteBinaryRequest(func_type);
  request_for_second_binary.set_code_token(second_load_response->code_token());
  VerifyResponse(roma_interface->ExecuteBinary(request_for_second_binary),
                 kNewUdfOutput);

  for (auto _ : state) {
    CHECK_OK(roma_interface->ExecuteBinary(request_for_second_binary));
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ExecuteBinarySyncUnaryGrpc(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  std::unique_ptr<RomaInterface> roma_interface =
      GetRomaInterface(mode, /*num_workers=*/state.range(2));
  auto load_response =
      roma_interface->LoadBinary(GetFileContent(kCPlusPlusBinaryPath));
  CHECK_OK(load_response);

  BinaryRequest::Function func_type =
      static_cast<BinaryRequest::Function>(state.range(1));
  ExecuteBinaryRequest request = ConstructExecuteBinaryRequest(func_type);
  request.set_code_token(load_response->code_token());
  CHECK_OK(roma_interface->ExecuteBinary(request));

  for (auto s : state) {
    CHECK_OK(roma_interface->ExecuteBinary(request));
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_GvisorCompareCPlusPlusAndGoLangBinary(benchmark::State& state) {
  Language lang = static_cast<Language>(state.range(0));
  std::unique_ptr<RomaInterface> roma_interface =
      GetRomaInterface(Mode::kModeGvisor, /*num_workers=*/2);

  auto load_response =
      roma_interface->LoadBinary(GetFileContent(GetFilePathFromLanguage(lang)));
  CHECK_OK(load_response);

  BinaryRequest::Function func_type =
      static_cast<BinaryRequest::Function>(state.range(1));
  ExecuteBinaryRequest request_for_binary =
      ConstructExecuteBinaryRequest(func_type);
  request_for_binary.set_code_token(load_response->code_token());
  VerifyResponse(
      roma_interface->ExecuteBinary(request_for_binary),
      lang == Language::kCPlusPlus ? kFirstUdfOutput : kGoBinaryOutput,
      func_type);

  for (auto _ : state) {
    CHECK_OK(roma_interface->ExecuteBinary(request_for_binary));
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
    ->ArgNames({"mode"})
    ->Iterations(2);

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

BENCHMARK(BM_ExecuteBinarySyncUnaryGrpc)
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
            BinaryRequest::
                FUNCTION_CONCURRENT_CALLBACKS,  // Concurrent invocations of
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
