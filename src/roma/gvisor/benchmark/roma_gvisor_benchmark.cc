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

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"
#include "src/roma/gvisor/udf/sample.pb.h"
#include "src/roma/gvisor/udf/sample_callback.pb.h"
#include "src/roma/gvisor/udf/sample_roma_gvisor_app_service.h"

namespace {
using ::google::scp::roma::FunctionBindingObjectV2;
using ::privacy_sandbox::sample_server::roma_app_api::GvisorSampleService;
using ::privacy_sandbox::sample_server::roma_app_api::SampleService;
using ::privacy_sandbox::server_common::gvisor::CallbackReadRequest;
using ::privacy_sandbox::server_common::gvisor::CallbackReadResponse;
using ::privacy_sandbox::server_common::gvisor::CallbackWriteRequest;
using ::privacy_sandbox::server_common::gvisor::CallbackWriteResponse;
using ::privacy_sandbox::server_common::gvisor::FUNCTION_CALLBACK;
using ::privacy_sandbox::server_common::gvisor::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::server_common::gvisor::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::server_common::gvisor::
    FUNCTION_TEN_CALLBACK_INVOCATIONS;
using ::privacy_sandbox::server_common::gvisor::FunctionType;
using ::privacy_sandbox::server_common::gvisor::Mode;
using ::privacy_sandbox::server_common::gvisor::ReadCallbackPayloadRequest;
using ::privacy_sandbox::server_common::gvisor::ReadCallbackPayloadResponse;
using ::privacy_sandbox::server_common::gvisor::RunPrimeSieveRequest;
using ::privacy_sandbox::server_common::gvisor::RunPrimeSieveResponse;
using ::privacy_sandbox::server_common::gvisor::SampleRequest;
using ::privacy_sandbox::server_common::gvisor::SampleResponse;
using ::privacy_sandbox::server_common::gvisor::SortListRequest;
using ::privacy_sandbox::server_common::gvisor::SortListResponse;
using ::privacy_sandbox::server_common::gvisor::WriteCallbackPayloadRequest;
using ::privacy_sandbox::server_common::gvisor::WriteCallbackPayloadResponse;

constexpr int kPrimeCount = 9592;
constexpr std::string_view kUdfPath = "/udf";
constexpr std::string_view kGoLangBinaryFilename = "sample_go_udf";
constexpr std::string_view kCPlusPlusBinaryFilename = "sample_udf";
constexpr std::string_view kCPlusPlusNewBinaryFilename = "new_udf";
constexpr std::string_view kPayloadUdfFilename = "payload_read_udf";
constexpr std::string_view kPayloadWriteUdfFilename = "payload_write_udf";
constexpr std::string_view kCallbackPayloadReadUdfFilename =
    "callback_payload_read_udf";
constexpr std::string_view kCallbackPayloadWriteUdfFilename =
    "callback_payload_write_udf";
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";

enum class Language {
  kCPlusPlus = 0,
  kGoLang = 1,
};

SampleResponse SendRequestAndGetResponse(
    GvisorSampleService<>& roma_service,
    privacy_sandbox::server_common::gvisor::FunctionType func_type,
    std::string_view code_token) {
  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(func_type);
  absl::StatusOr<std::unique_ptr<SampleResponse>> response;

  absl::Notification notif;
  CHECK_OK(roma_service.Sample(notif, bin_request, response,
                               /*metadata=*/{}, code_token));
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(1)));
  CHECK_OK(response);
  return *std::move((*response).get());
}

std::string LoadCode(GvisorSampleService<>& roma_service,
                     std::filesystem::path file_path) {
  absl::Notification notif;
  absl::Status notif_status;
  absl::StatusOr<std::string> code_id =
      roma_service.Register(file_path.string(), notif, notif_status);
  CHECK_OK(code_id);
  CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(10)));
  CHECK_OK(notif_status);
  return *std::move(code_id);
}

GvisorSampleService<> GetRomaService(Mode mode, int num_workers) {
  privacy_sandbox::server_common::gvisor::Config<> config = {
      .num_workers = num_workers,
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example", [](auto&) {}}},
  };
  absl::StatusOr<GvisorSampleService<>> sample_interface =
      GvisorSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  return std::move(*sample_interface);
}

void VerifyResponse(SampleResponse bin_response,
                    std::string_view expected_response,
                    FunctionType func = FUNCTION_HELLO_WORLD) {
  switch (func) {
    case FUNCTION_HELLO_WORLD:
      CHECK(absl::EqualsIgnoreCase(bin_response.greeting(), expected_response))
          << "Actual response: " << bin_response.greeting()
          << "\tExpected response: " << expected_response;
      break;
    case FUNCTION_PRIME_SIEVE:
      CHECK_EQ(bin_response.prime_number_size(), kPrimeCount)
          << "Expected " << kPrimeCount << " upto 100,000";
      break;
    default:
      LOG(ERROR) << "Unexpected input";
      abort();
  }
}

std::filesystem::path GetFilePathFromLanguage(Language lang) {
  const std::filesystem::path base_path(kUdfPath);
  switch (lang) {
    case Language::kCPlusPlus:
      return base_path / kCPlusPlusBinaryFilename;
    case Language::kGoLang:
      return base_path / kGoLangBinaryFilename;
    default:
      return std::filesystem::path();
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

std::string GetFunctionTypeStr(FunctionType func_type) {
  switch (func_type) {
    case FUNCTION_HELLO_WORLD:
      return R"(udf:"Hello World")";
    case FUNCTION_PRIME_SIEVE:
      return R"(udf:"Prime Sieve")";
    case FUNCTION_CALLBACK:
      return R"(udf:"Callback hook")";
    case FUNCTION_TEN_CALLBACK_INVOCATIONS:
      return R"(udf:"Ten callback invocations")";
    default:
      return "udf:Unknown";
  }
}

void ReadCallbackPayload(
    ::google::scp::roma::FunctionBindingPayload<>& wrapper) {
  CallbackReadRequest req;
  CHECK(req.ParseFromString(wrapper.io_proto.input_bytes()));
  int64_t payload_size = 0;
  for (const auto& p : req.payloads()) {
    payload_size += p.size();
  }
  CallbackReadResponse resp;
  resp.set_payload_size(payload_size);
  wrapper.io_proto.clear_input_bytes();
  resp.SerializeToString(wrapper.io_proto.mutable_output_bytes());
}

void WriteCallbackPayload(
    ::google::scp::roma::FunctionBindingPayload<>& wrapper) {
  CallbackWriteRequest req;
  CHECK(req.ParseFromString(wrapper.io_proto.input_bytes()));
  CallbackWriteResponse resp;
  auto* payloads = resp.mutable_payloads();
  payloads->Reserve(req.element_count());
  for (auto i = 0; i < req.element_count(); ++i) {
    payloads->Add(std::string(req.element_size(), 'a'));
  }
  wrapper.io_proto.clear_input_bytes();
  resp.SerializeToString(wrapper.io_proto.mutable_output_bytes());
}
}  // namespace

void BM_LoadBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  GvisorSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/1);
  FunctionType func_type = FUNCTION_HELLO_WORLD;

  const std::filesystem::path base_path(kUdfPath);
  auto bin_response = SendRequestAndGetResponse(
      roma_service, func_type,
      LoadCode(roma_service, base_path / kCPlusPlusBinaryFilename));
  VerifyResponse(bin_response, kFirstUdfOutput);

  std::string code_token;
  for (auto _ : state) {
    code_token =
        LoadCode(roma_service, base_path / kCPlusPlusNewBinaryFilename);
  }
  bin_response = SendRequestAndGetResponse(roma_service, func_type, code_token);
  VerifyResponse(bin_response, kNewUdfOutput);
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_Load2BinariesAndExecute1stBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  GvisorSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/2);

  const std::filesystem::path base_path(kUdfPath);
  std::string first_code_token =
      LoadCode(roma_service, base_path / kCPlusPlusBinaryFilename);
  std::string second_code_token =
      LoadCode(roma_service, base_path / kCPlusPlusNewBinaryFilename);

  FunctionType func_type = FUNCTION_HELLO_WORLD;
  VerifyResponse(
      SendRequestAndGetResponse(roma_service, func_type, first_code_token),
      kFirstUdfOutput);

  VerifyResponse(
      SendRequestAndGetResponse(roma_service, func_type, second_code_token),
      kNewUdfOutput);

  for (auto _ : state) {
    auto response =
        SendRequestAndGetResponse(roma_service, func_type, first_code_token);
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_Load2BinariesAndExecute2ndBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  GvisorSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/2);

  const std::filesystem::path base_path(kUdfPath);
  std::string first_code_token =
      LoadCode(roma_service, base_path / kCPlusPlusBinaryFilename);
  std::string second_code_token =
      LoadCode(roma_service, base_path / kCPlusPlusNewBinaryFilename);

  FunctionType func_type = FUNCTION_HELLO_WORLD;
  VerifyResponse(
      SendRequestAndGetResponse(roma_service, func_type, first_code_token),
      kFirstUdfOutput);

  VerifyResponse(
      SendRequestAndGetResponse(roma_service, func_type, second_code_token),
      kNewUdfOutput);

  for (auto _ : state) {
    auto response =
        SendRequestAndGetResponse(roma_service, func_type, second_code_token);
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ExecuteBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  GvisorSampleService<> roma_service =
      GetRomaService(mode, /*num_workers=*/state.range(2));

  const std::filesystem::path base_path(kUdfPath);
  std::string code_token =
      LoadCode(roma_service, base_path / kCPlusPlusBinaryFilename);

  FunctionType func_type = static_cast<FunctionType>(state.range(1));
  auto response =
      SendRequestAndGetResponse(roma_service, func_type, code_token);

  for (auto s : state) {
    response = SendRequestAndGetResponse(roma_service, func_type, code_token);
  }
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ExecuteBinaryCppVsGoLang(benchmark::State& state) {
  Language lang = static_cast<Language>(state.range(0));
  GvisorSampleService<> roma_service =
      GetRomaService(Mode::kModeGvisor, /*num_workers=*/2);

  std::string code_token =
      LoadCode(roma_service, GetFilePathFromLanguage(lang));

  FunctionType func_type = static_cast<FunctionType>(state.range(1));
  VerifyResponse(
      SendRequestAndGetResponse(roma_service, func_type, code_token),
      lang == Language::kCPlusPlus ? kFirstUdfOutput : kGoBinaryOutput,
      func_type);

  for (auto _ : state) {
    auto response =
        SendRequestAndGetResponse(roma_service, func_type, code_token);
  }
  state.SetLabel(absl::StrJoin(
      {GetFunctionTypeStr(func_type), GetLanguageStr(lang)}, ", "));
}

std::string GetSize(int64_t val) {
  int64_t divisor = 1;
  std::string_view unit_qual = "";
  if (val >= 1'000'000) {
    divisor = 1'000'000;
    unit_qual = "M";
  } else if (val >= 1000) {
    divisor = 1000;
    unit_qual = "K";
  }
  return absl::StrCat(val / divisor, unit_qual, "B");
}

void BM_ExecuteBinaryRequestPayload(benchmark::State& state) {
  int64_t elem_size = state.range(0);
  int64_t elem_count = state.range(1);
  Mode mode = static_cast<Mode>(state.range(2));
  GvisorSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/2);

  const auto rpc = [&roma_service](const auto& request,
                                   std::string_view code_token) {
    absl::StatusOr<std::unique_ptr<
        privacy_sandbox::server_common::gvisor::ReadPayloadResponse>>
        response;
    absl::Notification notif;
    CHECK_OK(roma_service.ReadPayload(notif, request, response,
                                      /*metadata=*/{}, code_token));
    CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(180)));
    return response;
  };

  privacy_sandbox::server_common::gvisor::ReadPayloadRequest request;
  std::string payload(elem_size, char(10));
  auto payloads = request.mutable_payloads();
  payloads->Reserve(elem_count);
  for (auto i = 0; i < elem_count; ++i) {
    payloads->Add(payload.data());
  }

  std::string code_tok = LoadCode(
      roma_service, std::filesystem::path(kUdfPath) / kPayloadUdfFilename);

  const int64_t payload_size = elem_size * elem_count;
  if (const auto response = rpc(request, code_tok); response.ok()) {
    CHECK((*response)->payload_size() == payload_size);
  } else {
    return;
  }

  for (auto _ : state) {
    (void)rpc(request, code_tok);
  }
  state.counters["elem_byte_size"] = elem_size;
  state.counters["elem_count"] = elem_count;
  state.counters["payload_size"] = payload_size;
  state.SetLabel(absl::StrCat("mode: ", GetModeStr(mode)));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          payload_size);
}

void BM_ExecuteBinaryResponsePayload(benchmark::State& state) {
  int64_t elem_size = state.range(0);
  int64_t elem_count = state.range(1);
  Mode mode = static_cast<Mode>(state.range(2));
  GvisorSampleService<> roma_service = GetRomaService(mode, /*num_workers=*/2);

  const auto rpc = [&roma_service](const auto& request,
                                   std::string_view code_token) {
    absl::StatusOr<std::unique_ptr<
        privacy_sandbox::server_common::gvisor::GeneratePayloadResponse>>
        response;
    absl::Notification notif;
    CHECK_OK(roma_service.GeneratePayload(notif, request, response,
                                          /*metadata=*/{}, code_token));
    CHECK(notif.WaitForNotificationWithTimeout(absl::Seconds(300)));
    return response;
  };

  privacy_sandbox::server_common::gvisor::GeneratePayloadRequest request;
  request.set_element_size(elem_size);
  request.set_element_count(elem_count);
  const int64_t req_payload_size = elem_size * elem_count;

  const std::filesystem::path base_path(kUdfPath);
  std::string code_tok =
      LoadCode(roma_service, base_path / kPayloadWriteUdfFilename);

  int64_t response_payload_size = 0;
  if (const auto response = rpc(request, code_tok); response.ok()) {
    for (const auto& p : (*response)->payloads()) {
      response_payload_size += p.size();
    }
    CHECK(req_payload_size == response_payload_size);
  } else {
    return;
  }

  for (auto _ : state) {
    (void)rpc(request, code_tok);
  }
  state.counters["elem_byte_size"] = elem_size;
  state.counters["elem_count"] = elem_count;
  state.counters["payload_size"] = req_payload_size;
  state.SetLabel(absl::StrCat("mode: ", GetModeStr(mode)));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          req_payload_size);
}

void BM_ExecuteBinaryCallbackRequestPayload(benchmark::State& state) {
  int64_t elem_size = state.range(0);
  int64_t elem_count = state.range(1);
  Mode mode = static_cast<Mode>(state.range(2));
  ::privacy_sandbox::server_common::gvisor::Config<> config = {
      .num_workers = 2,
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example",
                                                      ReadCallbackPayload}},
  };
  absl::StatusOr<GvisorSampleService<>> sample_interface =
      GvisorSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  GvisorSampleService<> roma_service = std::move(*sample_interface);

  const auto rpc = [&roma_service](std::string_view code_token,
                                   const auto& request) {
    absl::StatusOr<std::unique_ptr<
        privacy_sandbox::server_common::gvisor::ReadCallbackPayloadResponse>>
        response;
    absl::Notification notif;
    CHECK_OK(roma_service.ReadCallbackPayload(notif, request, response,
                                              /*metadata=*/{}, code_token));
    notif.WaitForNotification();
    return response;
  };

  privacy_sandbox::server_common::gvisor::ReadCallbackPayloadRequest request;
  request.set_element_size(elem_size);
  request.set_element_count(elem_count);
  const int64_t payload_size = elem_size * elem_count;

  const std::filesystem::path base_path(kUdfPath);
  std::string code_tok =
      LoadCode(roma_service, base_path / kCallbackPayloadReadUdfFilename);

  if (const auto response = rpc(code_tok, request); response.ok()) {
    CHECK((*response)->payload_size() == payload_size);
  } else {
    return;
  }

  for (auto _ : state) {
    (void)rpc(code_tok, request);
  }
  state.counters["elem_byte_size"] = elem_size;
  state.counters["elem_count"] = elem_count;
  state.counters["payload_size"] = payload_size;
  state.SetLabel(absl::StrCat("mode: ", GetModeStr(mode)));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          payload_size);
}

void BM_ExecuteBinaryCallbackResponsePayload(benchmark::State& state) {
  int64_t elem_size = state.range(0);
  int64_t elem_count = state.range(1);
  Mode mode = static_cast<Mode>(state.range(2));
  ::privacy_sandbox::server_common::gvisor::Config<> config = {
      .num_workers = 2,
      .roma_container_name = "roma_server",
      .function_bindings = {FunctionBindingObjectV2<>{"example",
                                                      WriteCallbackPayload}},
  };
  absl::StatusOr<GvisorSampleService<>> sample_interface =
      GvisorSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  GvisorSampleService<> roma_service = std::move(*sample_interface);

  const auto rpc = [&roma_service](std::string_view code_token,
                                   const auto& request) {
    absl::StatusOr<std::unique_ptr<
        privacy_sandbox::server_common::gvisor::WriteCallbackPayloadResponse>>
        response;
    absl::Notification notif;
    CHECK_OK(roma_service.WriteCallbackPayload(notif, request, response,
                                               /*metadata=*/{}, code_token));
    notif.WaitForNotification();
    return response;
  };

  privacy_sandbox::server_common::gvisor::WriteCallbackPayloadRequest request;
  request.set_element_size(elem_size);
  request.set_element_count(elem_count);
  const int64_t payload_size = elem_size * elem_count;

  const std::filesystem::path base_path(kUdfPath);
  std::string code_tok =
      LoadCode(roma_service, base_path / kCallbackPayloadWriteUdfFilename);

  if (const auto response = rpc(code_tok, request); response.ok()) {
    CHECK((*response)->payload_size() == payload_size);
  } else {
    return;
  }

  for (auto _ : state) {
    (void)rpc(code_tok, request);
  }
  state.counters["elem_byte_size"] = elem_size;
  state.counters["elem_count"] = elem_count;
  state.counters["payload_size"] = payload_size;
  state.SetLabel(absl::StrCat("mode: ", GetModeStr(mode)));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          payload_size);
}

void BM_ExecuteBinaryPrimeSieve(benchmark::State& state) {
  const Mode mode = static_cast<Mode>(state.range(0));
  ::privacy_sandbox::server_common::gvisor::Config<> config = {
      .num_workers = 2,
      .roma_container_name = "roma_server",
  };
  absl::StatusOr<GvisorSampleService<>> sample_interface =
      GvisorSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  GvisorSampleService<> roma_service = std::move(*sample_interface);
  const auto rpc = [&roma_service](std::string_view code_token,
                                   const auto& request) {
    absl::StatusOr<std::unique_ptr<RunPrimeSieveResponse>> response;
    absl::Notification notif;
    CHECK_OK(roma_service.RunPrimeSieve(notif, request, response,
                                        /*metadata=*/{}, code_token));
    notif.WaitForNotification();
    return response;
  };
  privacy_sandbox::server_common::gvisor::RunPrimeSieveRequest request;
  request.set_prime_count(state.range(1));
  const std::string code_tok = LoadCode(
      roma_service, std::filesystem::path(kUdfPath) / "prime_sieve_udf");
  {
    const auto response = rpc(code_tok, request);
    CHECK_OK(response);
    CHECK_GT((*response)->largest_prime(), 0);
  }
  for (auto _ : state) {
    CHECK_OK(rpc(code_tok, request));
  }
  state.SetLabel(absl::StrCat("mode: ", GetModeStr(mode)));
}

void BM_ExecuteBinarySortList(benchmark::State& state) {
  const Mode mode = static_cast<Mode>(state.range(0));
  ::privacy_sandbox::server_common::gvisor::Config<> config = {
      .num_workers = 2,
      .roma_container_name = "roma_server",
  };
  absl::StatusOr<GvisorSampleService<>> sample_interface =
      GvisorSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  GvisorSampleService<> roma_service = std::move(*sample_interface);
  const auto rpc = [&roma_service](std::string_view code_token,
                                   const auto& request) {
    absl::StatusOr<std::unique_ptr<SortListResponse>> response;
    absl::Notification notif;
    CHECK_OK(roma_service.SortList(notif, request, response,
                                   /*metadata=*/{}, code_token));
    notif.WaitForNotification();
    return response;
  };
  const std::string filename = [](int n_items) {
    switch (n_items) {
      case 10'000:
        return "sort_list_10k_udf";
      case 100'000:
        return "sort_list_100k_udf";
      case 1'000'000:
        return "sort_list_1m_udf";
      default:
        LOG(FATAL) << "Unrecognized n_items=" << n_items;
    }
  }(state.range(1));
  const std::string code_tok =
      LoadCode(roma_service, std::filesystem::path(kUdfPath) / filename);
  SortListRequest request;
  for (auto _ : state) {
    CHECK_OK(rpc(code_tok, request));
  }
  state.SetLabel(absl::StrCat("mode: ", GetModeStr(mode)));
}

BENCHMARK(BM_LoadBinary)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
    })
    ->ArgNames({"mode"});

BENCHMARK(BM_ExecuteBinaryCppVsGoLang)
    ->ArgsProduct({
        {
            (int)Language::kCPlusPlus,
            (int)Language::kGoLang,
        },
        {
            FUNCTION_HELLO_WORLD,  // Generic "Hello, world!"
            FUNCTION_PRIME_SIEVE,  // Sieve of primes
        },
    })
    ->ArgNames({"lang", "udf"});

BENCHMARK(BM_Load2BinariesAndExecute1stBinary)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
    })
    ->ArgNames({"mode"});

BENCHMARK(BM_Load2BinariesAndExecute2ndBinary)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
    })
    ->ArgNames({"mode"});

BENCHMARK(BM_ExecuteBinary)
    ->ArgsProduct({
        {
            (int)Mode::kModeGvisor,
            (int)Mode::kModeLocal,
        },
        {
            FUNCTION_HELLO_WORLD,               // Generic "Hello, world!"
            FUNCTION_PRIME_SIEVE,               // Sieve of primes
            FUNCTION_CALLBACK,                  // Generic callback hook
            FUNCTION_TEN_CALLBACK_INVOCATIONS,  // Ten invocations of generic
                                                // callback hook
        },
        {
            0, 1, 10, 20, 50, 100, 250  // Number of pre-warmed workers
        },
    })
    ->ArgNames({"mode", "udf", "num_workers"});

static void PayloadArguments(benchmark::internal::Benchmark* b) {
  constexpr int64_t kMaxPayloadSize = 50'000'000;
  constexpr int modes[] = {
      static_cast<int>(Mode::kModeGvisor),
      static_cast<int>(Mode::kModeLocal),
  };
  constexpr int64_t elem_counts[] = {1, 10, 100, 1'000};
  constexpr int64_t elem_sizes[] = {
      1,       1'000,   5'000,     10'000,    50'000,
      100'000, 500'000, 1'000'000, 5'000'000, 50'000'000,
  };
  for (auto mode : modes) {
    for (auto elem_count : elem_counts) {
      for (auto elem_size : elem_sizes) {
        if (elem_count * elem_size <= kMaxPayloadSize) {
          b->Args({elem_size, elem_count, mode});
        }
      }
    }
  }
}

BENCHMARK(BM_ExecuteBinaryRequestPayload)->Apply(PayloadArguments);
BENCHMARK(BM_ExecuteBinaryCallbackRequestPayload)->Apply(PayloadArguments);
BENCHMARK(BM_ExecuteBinaryCallbackResponsePayload)->Apply(PayloadArguments);

static void ResponsePayloadArgs(benchmark::internal::Benchmark* b) {
  constexpr int64_t kMaxPayloadSize = 50'000;
  constexpr int modes[] = {
      static_cast<int>(Mode::kModeGvisor),
      static_cast<int>(Mode::kModeLocal),
  };
  constexpr int64_t elem_counts[] = {1, 10, 100, 1'000};
  constexpr int64_t elem_sizes[] = {
      1, 1'000, 5'000, 10'000, 50'000,
  };
  for (auto mode : modes) {
    for (auto elem_count : elem_counts) {
      for (auto elem_size : elem_sizes) {
        if (elem_count * elem_size <= kMaxPayloadSize) {
          b->Args({elem_size, elem_count, mode});
        }
      }
    }
  }
}

BENCHMARK(BM_ExecuteBinaryResponsePayload)->Apply(ResponsePayloadArgs);
BENCHMARK(BM_ExecuteBinaryPrimeSieve)
    ->ArgsProduct({
        {
            static_cast<int>(Mode::kModeGvisor),
            static_cast<int>(Mode::kModeLocal),
        },
        {100'000, 500'000, 1'000'000, 5'000'000, 10'000'000},
    })
    ->ArgNames({"mode", "prime_count"});

BENCHMARK(BM_ExecuteBinarySortList)
    ->ArgsProduct({
        {
            static_cast<int>(Mode::kModeGvisor),
            static_cast<int>(Mode::kModeLocal),
        },
        {10'000, 100'000, 1'000'000},
    })
    ->ArgNames({"mode", "n_items"});

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
