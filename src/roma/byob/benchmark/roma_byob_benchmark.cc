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
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/sample_udf/sample_callback.pb.h"
#include "src/roma/byob/sample_udf/sample_roma_byob_app_service.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/roma/byob/utility/utils.h"

namespace {
using ::google::scp::roma::FunctionBindingObjectV2;
using ::privacy_sandbox::roma_byob::example::ByobSampleService;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::FunctionType;
using ::privacy_sandbox::roma_byob::example::LogRequest;
using ::privacy_sandbox::roma_byob::example::LogResponse;
using ::privacy_sandbox::roma_byob::example::RunPrimeSieveRequest;
using ::privacy_sandbox::roma_byob::example::RunPrimeSieveResponse;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;
using ::privacy_sandbox::roma_byob::example::SortListRequest;
using ::privacy_sandbox::roma_byob::example::SortListResponse;
using ::privacy_sandbox::server_common::byob::HasClonePermissionsByobWorker;
using ::privacy_sandbox::server_common::byob::Mode;

const std::filesystem::path kUdfPath = "/udf";
const std::filesystem::path kGoLangBinaryFilename = "sample_go_udf";
const std::filesystem::path kCPlusPlusBinaryFilename = "sample_udf";
const std::filesystem::path kCPlusPlusNewBinaryFilename = "new_udf";
const std::filesystem::path kJavaBinaryFilename = "sample_java_native_udf";
const std::filesystem::path kPayloadUdfFilename = "payload_read_udf";
const std::filesystem::path kPayloadWriteUdfFilename = "payload_write_udf";
constexpr int kPrimeCount = 9592;
constexpr std::string_view kFirstUdfOutput = "Hello, world!";
constexpr std::string_view kNewUdfOutput = "I am a new UDF!";
constexpr std::string_view kJavaOutput = "Hello, world from Java!";
constexpr std::string_view kGoBinaryOutput = "Hello, world from Go!";
constexpr Mode kModes[] = {
    Mode::kModeGvisorSandbox,
    Mode::kModeMinimalSandbox,
};

enum class Language {
  kCPlusPlus = 0,
  kGoLang = 1,
  kJava = 2,
};

enum class Log {
  kLogToDevNull = 0,
  kLogToFile = 1,
};

absl::StatusOr<SampleResponse> SendRequestAndGetResponse(
    ByobSampleService<>& roma_service,
    ::privacy_sandbox::roma_byob::example::FunctionType func_type,
    std::string_view code_token) {
  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(func_type);
  absl::StatusOr<std::unique_ptr<SampleResponse>> response;

  absl::Notification notif;
  if (auto execution_token =
          roma_service.Sample(notif, std::move(bin_request), response,
                              /*metadata=*/{}, code_token);
      !execution_token.ok()) {
    return std::move(execution_token).status();
  }
  CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
  if (!response.ok()) {
    return std::move(response).status();
  }
  return std::move(**response);
}

std::string LoadCode(ByobSampleService<>& roma_service,
                     std::filesystem::path file_path, bool enable_log_egress,
                     int num_workers) {
  absl::StatusOr<std::string> code_id;
  if (!enable_log_egress) {
    code_id = roma_service.Register(file_path, num_workers);
  } else {
    code_id = roma_service.RegisterForLogging(file_path, num_workers);
  }
  CHECK_OK(code_id);
  return *std::move(code_id);
}

ByobSampleService<> GetRomaService(
    Mode mode, ::privacy_sandbox::server_common::byob::Config<> config) {
  absl::StatusOr<ByobSampleService<>> sample_interface =
      ByobSampleService<>::Create(config, mode);
  CHECK_OK(sample_interface);
  return *std::move(sample_interface);
}

ByobSampleService<> GetRomaService(Mode mode) {
  ::privacy_sandbox::server_common::byob::Config<> config = {
      .roma_container_name = "roma_server",
  };
  return GetRomaService(mode, std::move(config));
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
      LOG(FATAL) << "Unexpected input";
  }
}

std::filesystem::path GetFilePathFromLanguage(Language lang) {
  switch (lang) {
    case Language::kCPlusPlus:
      return kUdfPath / kCPlusPlusBinaryFilename;
    case Language::kGoLang:
      return kUdfPath / kGoLangBinaryFilename;
    case Language::kJava:
      return kUdfPath / kJavaBinaryFilename;
    default:
      return std::filesystem::path();
  }
}

std::string GetModeStr(Mode mode) {
  switch (mode) {
    case Mode::kModeGvisorSandbox:
      return "mode:gVisor";
    case Mode::kModeMinimalSandbox:
      return "mode:Non-gVisor";
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
    case Language::kJava:
      return "language:Java";
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
    default:
      return "udf:Unknown";
  }
}

static void LoadArguments(benchmark::internal::Benchmark* b) {
  for (auto mode : kModes) {
    if (!HasClonePermissionsByobWorker(mode)) continue;
    b->Args({static_cast<int>(mode)});
  }
}

static void SampleBinaryArguments(benchmark::internal::Benchmark* b) {
  constexpr FunctionType function_types[] = {
      FUNCTION_HELLO_WORLD,  // Generic "Hello, world!"
      FUNCTION_PRIME_SIEVE,  // Sieve of primes
  };
  constexpr int64_t num_workers[] = {1, 10, 50, 100};
  for (auto mode : kModes) {
    if (!HasClonePermissionsByobWorker(mode)) continue;
    for (auto function_type : function_types) {
      for (auto num_worker : num_workers) {
        b->Args({static_cast<int>(mode), function_type, num_worker});
      }
    }
  }
}

static void ExecutePrimeSieveArguments(benchmark::internal::Benchmark* b) {
  constexpr int64_t prime_counts[] = {100'000, 500'000, 1'000'000, 5'000'000,
                                      10'000'000};
  for (auto mode : kModes) {
    if (!HasClonePermissionsByobWorker(mode)) continue;
    for (auto prime_count : prime_counts) {
      b->Args({static_cast<int>(mode), prime_count});
    }
  }
}

static void ExecuteSortListArguments(benchmark::internal::Benchmark* b) {
  constexpr int64_t n_items_counts[] = {10'000, 100'000, 1'000'000};
  for (auto mode : kModes) {
    if (!HasClonePermissionsByobWorker(mode)) continue;
    for (auto n_items_count : n_items_counts) {
      b->Args({static_cast<int>(mode), n_items_count});
    }
  }
}

static void PayloadArguments(benchmark::internal::Benchmark* b) {
  constexpr int64_t kMaxPayloadSize = 50'000'000;
  constexpr int64_t elem_counts[] = {1, 10, 100, 1'000};
  constexpr int64_t elem_sizes[] = {
      1,       1'000,   5'000,     10'000,    50'000,
      100'000, 500'000, 1'000'000, 5'000'000, 50'000'000,
  };
  for (auto mode : kModes) {
    if (!HasClonePermissionsByobWorker(mode)) continue;
    for (auto elem_count : elem_counts) {
      for (auto elem_size : elem_sizes) {
        if (elem_count * elem_size <= kMaxPayloadSize) {
          b->Args({elem_size, elem_count, static_cast<int>(mode)});
        }
      }
    }
  }
}
}  // namespace

void BM_LoadBinary(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  ByobSampleService<> roma_service = GetRomaService(mode);
  FunctionType func_type = FUNCTION_HELLO_WORLD;

  auto bin_response = SendRequestAndGetResponse(
      roma_service, func_type,
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename,
               /*enable_log_egress=*/false,
               /*num_workers=*/1));
  CHECK_OK(bin_response);
  VerifyResponse(*bin_response, kFirstUdfOutput);

  std::string code_token;
  for (auto _ : state) {
    code_token = LoadCode(roma_service, kUdfPath / kCPlusPlusNewBinaryFilename,
                          /*enable_log_egress=*/false,
                          /*num_workers=*/1);
  }
  bin_response = SendRequestAndGetResponse(roma_service, func_type, code_token);
  CHECK_OK(bin_response);
  VerifyResponse(*bin_response, kNewUdfOutput);
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ProcessRequest(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  ByobSampleService<> roma_service = GetRomaService(mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename,
               /*enable_log_egress=*/false,
               /*num_workers=*/state.range(2));

  FunctionType func_type = static_cast<FunctionType>(state.range(1));

  int failure_count = 0;
  for (auto _ : state) {
    if (!SendRequestAndGetResponse(roma_service, func_type, code_token).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ProcessRequestUsingCallback(benchmark::State& state) {
  Mode mode = static_cast<Mode>(state.range(0));
  ByobSampleService<> roma_service = GetRomaService(mode);

  std::string code_token =
      LoadCode(roma_service, kUdfPath / kCPlusPlusBinaryFilename,
               /*enable_log_egress=*/false,
               /*num_workers=*/state.range(2));

  FunctionType func_type = static_cast<FunctionType>(state.range(1));
  // Data we are sending to the server.
  SampleRequest bin_request;
  bin_request.set_function(func_type);

  const auto rpc = [&roma_service](const auto& bin_request,
                                   std::string_view code_token) {
    absl::Notification notif;
    absl::StatusOr<SampleResponse> bin_response;
    auto callback = [&notif,
                     &bin_response](absl::StatusOr<SampleResponse> resp) {
      bin_response = std::move(resp);
      notif.Notify();
    };
    if (!roma_service
             .Sample(callback, bin_request,
                     /*metadata=*/{}, code_token)
             .ok()) {
      return false;
    }
    CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
    CHECK_OK(bin_response);
    return true;
  };

  int failure_count = 0;
  for (auto _ : state) {
    if (!rpc(bin_request, code_token)) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
  state.SetLabel(
      absl::StrJoin({GetModeStr(mode), GetFunctionTypeStr(func_type)}, ", "));
}

void BM_ProcessRequestMultipleLanguages(benchmark::State& state) {
  Language lang = static_cast<Language>(state.range(0));
  std::string mounts = "";
  if (lang == Language::kJava) {
    mounts = "/proc";
#if defined(__aarch64__)
    // TODO: b/377349908 - Enable Java benchmarks post-ARM64 fix
    state.SkipWithError("Skipping Java test on ARM64");
    return;
#endif
  } else if (lang == Language::kGoLang) {
    mounts = "";
  } else {
    mounts = LIB_MOUNTS;
  }
  ::privacy_sandbox::server_common::byob::Config<> config = {
      .roma_container_name = "roma_server",
      .lib_mounts = std::move(mounts),
  };
  ByobSampleService<> roma_service =
      GetRomaService(Mode::kModeGvisorSandbox, std::move(config));

  std::string code_token =
      LoadCode(roma_service, GetFilePathFromLanguage(lang),
               /*enable_log_egress=*/false, /*num_workers=*/2);

  FunctionType func_type = static_cast<FunctionType>(state.range(1));
  std::string expected_response;
  if (lang == Language::kCPlusPlus) {
    expected_response = kFirstUdfOutput;
  } else if (lang == Language::kJava) {
    expected_response = kJavaOutput;
  } else if (lang == Language::kGoLang) {
    expected_response = kGoBinaryOutput;
  } else {
    LOG(FATAL) << "lang (" << static_cast<int>(lang) << ") not recognized.";
  }
  {
    const auto response =
        SendRequestAndGetResponse(roma_service, func_type, code_token);
    CHECK_OK(response);
    VerifyResponse(*response, expected_response, func_type);
  }

  int failure_count = 0;
  for (auto _ : state) {
    if (!SendRequestAndGetResponse(roma_service, func_type, code_token).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
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

void BM_ProcessRequestRequestPayload(benchmark::State& state) {
  int64_t elem_size = state.range(0);
  int64_t elem_count = state.range(1);
  Mode mode = static_cast<Mode>(state.range(2));
  ByobSampleService<> roma_service = GetRomaService(mode);

  const auto rpc = [&roma_service](const auto& request,
                                   std::string_view code_token)
      -> absl::StatusOr<std::unique_ptr<
          ::privacy_sandbox::roma_byob::example::ReadPayloadResponse>> {
    absl::StatusOr<std::unique_ptr<
        ::privacy_sandbox::roma_byob::example::ReadPayloadResponse>>
        response;
    absl::Notification notif;
    if (auto execution_token =
            roma_service.ReadPayload(notif, request, response,
                                     /*metadata=*/{}, code_token);
        !execution_token.ok()) {
      return std::move(execution_token).status();
    }
    CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(5)));
    return response;
  };

  ::privacy_sandbox::roma_byob::example::ReadPayloadRequest request;
  std::string payload(elem_size, char(10));
  auto payloads = request.mutable_payloads();
  payloads->Reserve(elem_count);
  for (auto i = 0; i < elem_count; ++i) {
    payloads->Add(payload.data());
  }

  std::string code_tok =
      LoadCode(roma_service, kUdfPath / kPayloadUdfFilename,
               /*enable_log_egress=*/false, /*num_workers=*/2);

  const int64_t payload_size = elem_size * elem_count;
  if (const auto response = rpc(request, code_tok); response.ok()) {
    CHECK((*response)->payload_size() == payload_size);
  } else {
    return;
  }

  int failure_count = 0;
  for (auto _ : state) {
    if (!rpc(request, code_tok).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
  state.counters["elem_byte_size"] = elem_size;
  state.counters["elem_count"] = elem_count;
  state.counters["payload_size"] = payload_size;
  state.SetLabel(GetModeStr(mode));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          payload_size);
}

void BM_ProcessRequestResponsePayload(benchmark::State& state) {
  int64_t elem_size = state.range(0);
  int64_t elem_count = state.range(1);
  Mode mode = static_cast<Mode>(state.range(2));
  ByobSampleService<> roma_service = GetRomaService(mode);

  const auto rpc = [&roma_service](const auto& request,
                                   std::string_view code_token)
      -> absl::StatusOr<std::unique_ptr<
          ::privacy_sandbox::roma_byob::example::GeneratePayloadResponse>> {
    absl::StatusOr<std::unique_ptr<
        ::privacy_sandbox::roma_byob::example::GeneratePayloadResponse>>
        response;
    absl::Notification notif;
    if (auto execution_token =
            roma_service.GeneratePayload(notif, request, response,
                                         /*metadata=*/{}, code_token);
        !execution_token.ok()) {
      return std::move(execution_token).status();
    }
    CHECK(notif.WaitForNotificationWithTimeout(absl::Minutes(10)));
    return response;
  };

  ::privacy_sandbox::roma_byob::example::GeneratePayloadRequest request;
  request.set_element_size(elem_size);
  request.set_element_count(elem_count);
  const int64_t req_payload_size = elem_size * elem_count;

  std::string code_tok =
      LoadCode(roma_service, kUdfPath / kPayloadWriteUdfFilename,
               /*enable_log_egress=*/false, /*num_workers=*/2);

  int64_t response_payload_size = 0;
  if (const auto response = rpc(request, code_tok); response.ok()) {
    for (const auto& p : (*response)->payloads()) {
      response_payload_size += p.size();
    }
    CHECK(req_payload_size == response_payload_size);
  } else {
    return;
  }

  int failure_count = 0;
  for (auto _ : state) {
    if (!rpc(request, code_tok).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
  state.counters["elem_byte_size"] = elem_size;
  state.counters["elem_count"] = elem_count;
  state.counters["payload_size"] = req_payload_size;
  state.SetLabel(GetModeStr(mode));
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          req_payload_size);
}

void BM_ProcessRequestPrimeSieve(benchmark::State& state) {
  const Mode mode = static_cast<Mode>(state.range(0));
  ByobSampleService<> roma_service = GetRomaService(mode);
  const auto rpc = [&roma_service](std::string_view code_token,
                                   const auto& request)
      -> absl::StatusOr<std::unique_ptr<RunPrimeSieveResponse>> {
    absl::StatusOr<std::unique_ptr<RunPrimeSieveResponse>> response;
    absl::Notification notif;
    if (auto execution_token =
            roma_service.RunPrimeSieve(notif, request, response,
                                       /*metadata=*/{}, code_token);
        !execution_token.ok()) {
      return std::move(execution_token).status();
    }
    notif.WaitForNotification();
    return response;
  };
  ::privacy_sandbox::roma_byob::example::RunPrimeSieveRequest request;
  request.set_prime_count(state.range(1));
  const std::string code_tok = LoadCode(
      roma_service, std::filesystem::path(kUdfPath) / "prime_sieve_udf",
      /*enable_log_egress=*/false,
      /*num_workers=*/2);
  {
    const auto response = rpc(code_tok, request);
    CHECK_OK(response);
    CHECK_GT((*response)->largest_prime(), 0);
  }
  int failure_count = 0;
  for (auto _ : state) {
    if (!rpc(code_tok, request).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
  state.SetLabel(GetModeStr(mode));
}

void BM_ProcessRequestSortList(benchmark::State& state) {
  const Mode mode = static_cast<Mode>(state.range(0));
  ByobSampleService<> roma_service = GetRomaService(mode);
  const auto rpc = [&roma_service](std::string_view code_token,
                                   const auto& request) {
    absl::StatusOr<std::unique_ptr<SortListResponse>> response;
    absl::Notification notif;
    if (auto execution_token =
            roma_service.SortList(notif, request, response,
                                  /*metadata=*/{}, code_token);
        !execution_token.ok()) {
      return std::move(execution_token).status();
    }
    notif.WaitForNotification();
    CHECK_OK(response);
    return absl::OkStatus();
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
      LoadCode(roma_service, std::filesystem::path(kUdfPath) / filename,
               /*enable_log_egress=*/false,
               /*num_workers=*/2);
  SortListRequest request;
  // Add failure counter.
  int failure_count = 0;
  for (auto _ : state) {
    if (!rpc(code_tok, request).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
  state.SetLabel(GetModeStr(mode));
}

void BM_ProcessRequestDevNullVsLogBinary(benchmark::State& state) {
  const Log log = static_cast<Log>(state.range(0));
  ByobSampleService<> roma_service = GetRomaService(Mode::kModeGvisorSandbox);
  const bool enable_log_egress = [](Log log) {
    switch (log) {
      case Log::kLogToDevNull:
        return false;
      case Log::kLogToFile:
        return true;
      default:
        LOG(FATAL) << "Unrecognized log=" << static_cast<int>(log);
    }
  }(log);
  const auto rpc = [&roma_service, &enable_log_egress](
                       std::string_view code_token, const auto& request) {
    absl::Notification exec_notif;
    absl::StatusOr<LogResponse> bin_response;
    auto callback = [&exec_notif, &bin_response, &enable_log_egress](
                        absl::StatusOr<LogResponse> resp,
                        absl::StatusOr<std::string_view> logs) {
      bin_response = std::move(resp);
      if (enable_log_egress) {
        // TODO(b/380273785): Find out why this check fails sometimes.
        CHECK_OK(logs);
        CHECK(absl::StartsWith(*logs, "I am benchmark stderr log.")) << *logs;
      } else {
        CHECK(!logs.ok());
      }
      exec_notif.Notify();
    };
    if (auto execution_token = roma_service.Log(callback, request,
                                                /*metadata=*/{}, code_token);
        !execution_token.ok()) {
      return std::move(execution_token).status();
    }
    CHECK(exec_notif.WaitForNotificationWithTimeout(absl::Minutes(1)));
    CHECK_OK(bin_response);
    return absl::OkStatus();
  };

  const std::string code_token = LoadCode(
      roma_service, std::filesystem::path(kUdfPath) / "log_benchmark_udf",
      enable_log_egress, /*num_workers=*/10);
  ::sleep(/*seconds=*/5);
  LogRequest request;
  request.set_log_count(state.range(1));
  int failure_count = 0;
  for (auto _ : state) {
    if (!rpc(code_token, request).ok()) {
      ++failure_count;
    }
  }
  state.counters["failure_rate"] =
      benchmark::Counter(failure_count, benchmark::Counter::kAvgIterations);
}

BENCHMARK(BM_LoadBinary)->Apply(LoadArguments)->ArgNames({"mode"});
BENCHMARK(BM_ProcessRequestMultipleLanguages)
    ->ArgsProduct({
        {
            (int)Language::kCPlusPlus,
            (int)Language::kJava,
            (int)Language::kGoLang,
        },
        {
            FUNCTION_HELLO_WORLD,  // Generic "Hello, world!"
            FUNCTION_PRIME_SIEVE,  // Sieve of primes
        },
    })
    ->ArgNames({"lang", "udf"});

BENCHMARK(BM_ProcessRequest)
    ->Apply(SampleBinaryArguments)
    ->ArgNames({"mode", "udf", "num_workers"});

BENCHMARK(BM_ProcessRequestUsingCallback)
    ->Apply(SampleBinaryArguments)
    ->ArgNames({"mode", "udf", "num_workers"});

BENCHMARK(BM_ProcessRequestRequestPayload)->Apply(PayloadArguments);
BENCHMARK(BM_ProcessRequestResponsePayload)->Apply(PayloadArguments);
BENCHMARK(BM_ProcessRequestPrimeSieve)
    ->Apply(ExecutePrimeSieveArguments)
    ->ArgNames({"mode", "prime_count"});

BENCHMARK(BM_ProcessRequestSortList)
    ->Apply(ExecuteSortListArguments)
    ->ArgNames({"mode", "n_items"});

BENCHMARK(BM_ProcessRequestDevNullVsLogBinary)
    ->ArgsProduct({{
                       (int)Log::kLogToFile,
                       (int)Log::kLogToDevNull,
                   },
                   {
                       10,
                       100,
                       1000,
                       10'000,
                   }})
    ->ArgNames({"log", "num_logs"});

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  return 0;
}
