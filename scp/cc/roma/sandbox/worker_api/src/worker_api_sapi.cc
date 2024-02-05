/*
 * Copyright 2023 Google LLC
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

#include "worker_api_sapi.h"

#include <memory>
#include <string>
#include <utility>

#include "public/core/interface/execution_result.h"
#include "roma/sandbox/constants/constants.h"
#include "src/cpp/util/duration.h"
#include "src/cpp/util/protoutil.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_API_INVALID_DURATION;

namespace google::scp::roma::sandbox::worker_api {
WorkerApiSapi::WorkerApiSapi(const WorkerApiSapiConfig& config)
    : sandbox_api_(
          config.js_engine_require_code_preload,
          config.native_js_function_comms_fd, config.native_js_function_names,
          config.max_worker_virtual_memory_mb,
          config.js_engine_resource_constraints.initial_heap_size_in_mb,
          config.js_engine_resource_constraints.maximum_heap_size_in_mb,
          config.js_engine_max_wasm_memory_number_of_pages,
          config.sandbox_request_response_shared_buffer_size_mb,
          config.enable_sandbox_sharing_request_response_with_buffer_only) {}

absl::Status WorkerApiSapi::Init() noexcept {
  absl::MutexLock lock(&run_code_mutex_);
  return sandbox_api_.Init();
}

absl::Status WorkerApiSapi::Run() noexcept {
  absl::MutexLock lock(&run_code_mutex_);
  return sandbox_api_.Run();
}

absl::Status WorkerApiSapi::Stop() noexcept {
  absl::MutexLock lock(&run_code_mutex_);
  return sandbox_api_.Stop();
}

ExecutionResultOr<WorkerApi::RunCodeResponse> WorkerApiSapi::RunCode(
    const WorkerApi::RunCodeRequest& request) noexcept {
  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(std::string(request.code));
  auto input_type =
      request.metadata.find(google::scp::roma::sandbox::constants::kInputType);
  if (input_type != request.metadata.end() &&
      input_type->second ==
          google::scp::roma::sandbox::constants::kInputTypeBytes) {
    params_proto.set_input_bytes(request.input.at(0));
  } else {
    params_proto.mutable_input_strings()->mutable_inputs()->Add(
        request.input.begin(), request.input.end());
  }
  params_proto.set_wasm(std::string(request.wasm.begin(), request.wasm.end()));
  for (auto&& kv : request.metadata) {
    (*params_proto.mutable_metadata())[kv.first] = kv.second;
  }

  privacy_sandbox::server_common::Stopwatch stopwatch;
  core::ExecutionResult result;
  {
    // Only this block is mutex protected because everything else is dealing
    // with input and output arguments, which is threadsafe.
    absl::MutexLock lock(&run_code_mutex_);
    result = sandbox_api_.RunCode(params_proto);
  }

  if (!result.Successful()) {
    return result;
  }

  WorkerApi::RunCodeResponse code_response;
  code_response.metrics[roma::sandbox::constants::
                            kExecutionMetricSandboxedJsEngineCallDuration] =
      stopwatch.GetElapsedTime();
  for (auto& kv : params_proto.metrics()) {
    auto duration =
        privacy_sandbox::server_common::DecodeGoogleApiProto(kv.second);
    if (!duration.ok()) {
      return FailureExecutionResult(SC_ROMA_WORKER_API_INVALID_DURATION);
    }
    code_response.metrics[kv.first] = std::move(duration).value();
  }
  code_response.response =
      std::make_shared<std::string>(std::move(params_proto.response()));
  return code_response;
}

void WorkerApiSapi::Terminate() noexcept {
  absl::MutexLock lock(&run_code_mutex_);
  sandbox_api_.Terminate();
}
}  // namespace google::scp::roma::sandbox::worker_api
