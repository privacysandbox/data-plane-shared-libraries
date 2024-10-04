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

#include <stdint.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_join.h"
#include "src/roma/config/config.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_function_binding.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_js_engine.h"
#include "src/roma/sandbox/worker/worker.h"
#include "src/roma/sandbox/worker_api/sapi/utils.h"
#include "src/roma/sandbox/worker_api/sapi/worker_init_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/util/duration.h"
#include "src/util/protoutil.h"

#include "worker_wrapper.h"

using google::scp::roma::JsEngineResourceConstraints;
using google::scp::roma::sandbox::constants::
    kExecutionMetricJsEngineCallDuration;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupV8FlagsKey;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateFunctionBinding;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::sandbox::worker::Worker;

namespace google::scp::roma::sandbox::worker_api {
absl::Status WorkerWrapper::Init(
    ::worker_api::WorkerInitParamsProto& init_params) {
  if (worker_) {
    absl::Status status = Stop();
    // If we fail to stop the previous worker then log but keep going because
    // we'll be recreating it momentarily.
    if (status != absl::OkStatus()) {
      ROMA_VLOG(1) << status << "Failed to stop previous worker";
    }
  }

  std::vector<std::string> native_js_function_names(
      init_params.native_js_function_names().begin(),
      init_params.native_js_function_names().end());

  std::vector<std::string> rpc_method_names(
      init_params.rpc_method_names().begin(),
      init_params.rpc_method_names().end());

  std::vector<std::string> v8_flags(init_params.v8_flags().begin(),
                                    init_params.v8_flags().end());

  JsEngineResourceConstraints resource_constraints;
  resource_constraints.initial_heap_size_in_mb =
      static_cast<size_t>(init_params.js_engine_initial_heap_size_mb());
  resource_constraints.maximum_heap_size_in_mb =
      static_cast<size_t>(init_params.js_engine_maximum_heap_size_mb());

  V8WorkerEngineParams v8_params = {
      .native_js_function_comms_fd = native_js_function_comms_fd_,
      .native_js_function_names = std::move(native_js_function_names),
      .rpc_method_names = std::move(rpc_method_names),
      .server_address = init_params.server_address(),
      .resource_constraints = resource_constraints,
      .max_wasm_memory_number_of_pages = static_cast<size_t>(
          init_params.js_engine_max_wasm_memory_number_of_pages()),
      .require_preload = init_params.require_code_preload_for_execution(),
      .skip_v8_cleanup = true,
      .v8_flags = std::move(v8_flags),
      .enable_profilers = init_params.enable_profilers(),
      .logging_function_set = init_params.logging_function_set(),
      .disable_udf_stacktraces_in_response =
          init_params.disable_udf_stacktraces_in_response(),
  };

  worker_ = CreateWorker(v8_params);

  ROMA_VLOG(1) << "Worker wrapper successfully created the worker";
  return absl::OkStatus();
}

bool WorkerWrapper::SandboxIsInitialized() { return true; }

std::pair<absl::Status, RetryStatus> WorkerWrapper::InternalRunCode(
    ::worker_api::WorkerParamsProto& params) {
  if (!worker_) {
    return WrapResultWithNoRetry(absl::FailedPreconditionError(
        "A call to run code was issued with an uninitialized worker"));
  }
  const auto& code = params.code();

  // WorkerParamsProto one of for `input_strings` or `input_bytes` or neither.
  std::vector<std::string_view> input;
  auto input_type =
      params.metadata().find(google::scp::roma::sandbox::constants::kInputType);
  if (input_type != params.metadata().end() &&
      input_type->second ==
          google::scp::roma::sandbox::constants::kInputTypeBytes) {
    input.push_back(params.input_bytes());
  } else {
    input.reserve(params.input_strings().inputs_size());
    for (int i = 0; i < params.input_strings().inputs_size(); i++) {
      input.push_back(params.input_strings().inputs().at(i));
    }
  }
  const absl::flat_hash_map<std::string_view, std::string_view> metadata(
      params.metadata().begin(), params.metadata().end());
  auto wasm_bin = reinterpret_cast<const uint8_t*>(params.wasm().c_str());
  absl::Span<const uint8_t> wasm =
      absl::MakeConstSpan(wasm_bin, params.wasm().length());
  privacy_sandbox::server_common::Stopwatch stopwatch;
  auto response_or = worker_->RunCode(code, input, metadata, wasm);
  auto js_duration = privacy_sandbox::server_common::EncodeGoogleApiProto(
      stopwatch.GetElapsedTime());
  if (!js_duration.ok()) {
    return WrapResultWithNoRetry(
        absl::InvalidArgumentError("Failed to convert a duration"));
  }
  (*params.mutable_metrics())[kExecutionMetricJsEngineCallDuration] =
      std::move(js_duration).value();
  if (!response_or.ok()) {
    params.set_error_message(std::string(response_or.status().message()));
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InternalError("Execution failed"));
    builder << params.error_message();
    return WrapResultWithNoRetry(builder);
  }

  for (const auto& pair : response_or.value().metrics) {
    auto duration =
        privacy_sandbox::server_common::EncodeGoogleApiProto(pair.second);
    if (!duration.ok()) {
      return WrapResultWithNoRetry(
          absl::InvalidArgumentError("Failed to convert a duration"));
    }
    (*params.mutable_metrics())[pair.first] = std::move(duration).value();
  }

  params.set_response(std::move(response_or.value().response));
  params.set_profiler_output(std::move(response_or.value().profiler_output));
  return WrapResultWithNoRetry(absl::OkStatus());
}

absl::Status WorkerWrapper::Run() {
  if (!worker_) {
    return absl::FailedPreconditionError(
        "A call to run code was issued with an uninitialized worker");
  }
  worker_->Run();
  return absl::OkStatus();
}

absl::Status WorkerWrapper::Stop() {
  if (!worker_) {
    return absl::FailedPreconditionError(
        "A call to run code was issued with an uninitialized worker");
  }
  worker_->Stop();
  worker_.reset();
  return absl::OkStatus();
}

std::pair<absl::Status, RetryStatus> WorkerWrapper::RunCode(
    ::worker_api::WorkerParamsProto& params) {
  ROMA_VLOG(1)
      << "Worker wrapper RunCodeFromSerializedData() received the request"
      << std::endl;
  const auto result = InternalRunCode(params);

  if (result.first != absl::OkStatus() && params.error_message().empty()) {
    return result;
  }

  // Don't return the input or code.
  ClearInputFields(params);

  ROMA_VLOG(1) << "Worker wrapper successfully executed the request"
               << std::endl;
  return result;
}

void WorkerWrapper::Terminate() {}
}  // namespace google::scp::roma::sandbox::worker_api
