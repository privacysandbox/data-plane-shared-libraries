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

#include <sys/wait.h>
#include <unistd.h>
#include <v8.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <numeric>  // For std::accumulate
#include <string>
#include <vector>

#include <benchmark/benchmark.h>
#include <libplatform/libplatform.h>
#include <nlohmann/json.hpp>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::CodeObject;
using google::scp::roma::Config;
using google::scp::roma::InvocationStrRequest;
using google::scp::roma::ResponseObject;
using google::scp::roma::sandbox::roma_service::RomaService;

constexpr std::string_view kPrimeSieveUdfPath =
    "src/roma/tools/v8_cli/test_udfs/primeSieve.js";
constexpr std::string_view kPrimeSieveUdfHandlerName = "primes";

namespace {
std::string MakeLabel(const std::string& name) {
  nlohmann::ordered_json json;
  json["perfgate_label"] = name;
  return json.dump();
}

std::string GetUDF() {
  std::ifstream file(kPrimeSieveUdfPath.data());
  CHECK(file.is_open()) << "Failed to open file: " << kPrimeSieveUdfPath;
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  return content;
}

std::string GetFunctionCall(int runs, int cycles, int prime_sieve_limit) {
  return absl::StrCat(kPrimeSieveUdfHandlerName, "(", runs, ", ", cycles, ", ",
                      prime_sieve_limit, ");");
}

void BM_PrimeSieveUdfRoma(::benchmark::State& state) {
  state.SetLabel(MakeLabel(state.name()));
  Config<> config;
  config.number_of_workers = 2;
  RomaService<> roma_service(std::move(config));
  CHECK_OK(roma_service.Init());
  auto escape_input = [](int n) { return absl::StrCat("\"", n, "\""); };
  std::string udf = GetUDF();
  {
    absl::Notification done;
    CHECK_OK(roma_service.LoadCodeObj(std::make_unique<CodeObject>(CodeObject{
                                          .id = "foo",
                                          .version_string = "v1",
                                          .js = udf,
                                      }),
                                      [&](absl::StatusOr<ResponseObject> resp) {
                                        CHECK_OK(resp);
                                        done.Notify();
                                      }));
    done.WaitForNotification();
  }
  const InvocationStrRequest<> request{
      .id = "foo",
      .version_string = "v1",
      .handler_name = std::string(kPrimeSieveUdfHandlerName),
      .input = {escape_input(state.range(0)), escape_input(state.range(1)),
                escape_input(state.range(2))}};
  std::vector<double> results;
  for (auto _ : state) {
    absl::Notification done;
    std::string result;
    CHECK_OK(
        roma_service.Execute(std::make_unique<InvocationStrRequest<>>(request),
                             [&](absl::StatusOr<ResponseObject> resp) {
                               CHECK_OK(resp);
                               result = std::move(resp->resp);
                               done.Notify();
                             }));
    done.WaitForNotification();
    state.PauseTiming();
    double ms;
    CHECK(absl::SimpleAtod(result, &ms))
        << "Failed to parse result: " << result;
    results.push_back(ms);
  }
  double avg =
      std::accumulate(results.begin(), results.end(), 0.0) / results.size();
  state.SetLabel(absl::StrCat("JS Duration: ", avg, " ms"));
  CHECK_OK(roma_service.Stop());
}

void BM_PrimeSieveUdfV8(::benchmark::State& state) {
  state.SetLabel(MakeLabel(state.name()));
  const std::string udf = GetUDF();
  const std::string function_call =
      GetFunctionCall(state.range(0), state.range(1), state.range(2));
  const std::string js_code = absl::StrCat(udf, "\n", function_call);
  std::vector<double> results;

  v8::Isolate::CreateParams create_params;
  create_params.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  v8::Isolate* isolate = v8::Isolate::New(create_params);
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);
    v8::Context::Scope context_scope(context);

    v8::Local<v8::String> js_source =
        v8::String::NewFromUtf8(isolate, js_code.c_str()).ToLocalChecked();
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, js_source).ToLocalChecked();
    for (auto _ : state) {
      v8::Local<v8::Value> result = script->Run(context).ToLocalChecked();
      state.PauseTiming();
      v8::String::Utf8Value utf8(isolate, result);
      std::string result_str = *utf8;
      double ms;
      CHECK(absl::SimpleAtod(result_str, &ms))
          << "Failed to parse result: " << result_str;
      results.push_back(ms);
      state.ResumeTiming();
    }
  }

  double avg =
      std::accumulate(results.begin(), results.end(), 0.0) / results.size();
  state.SetLabel(absl::StrCat("JS Duration: ", avg, " ms"));
}
}  // namespace

BENCHMARK(BM_PrimeSieveUdfRoma)->Args({1'000, 10, 10'000});
BENCHMARK(BM_PrimeSieveUdfV8)->Args({1'000, 10, 10'000});

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  v8::V8::InitializeICUDefaultLocation("");
  v8::V8::InitializeExternalStartupData("");
  std::unique_ptr<v8::Platform> platform = v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(platform.get());
  v8::V8::Initialize();

  benchmark::Initialize(&argc, argv);
  benchmark::RunSpecifiedBenchmarks();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  return 0;
}
