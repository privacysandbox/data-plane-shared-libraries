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

#include "worker_wrapper_impl.h"

#include <stdint.h>
#include <unistd.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_join.h"
#include "sandboxed_api/lenval_core.h"
#include "sandboxed_api/sandbox2/buffer.h"
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

using google::scp::roma::JsEngineResourceConstraints;
using google::scp::roma::sandbox::constants::kBadFd;
using google::scp::roma::sandbox::constants::
    kExecutionMetricJsEngineCallDuration;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupV8FlagsKey;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateFunctionBinding;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::sandbox::worker::Worker;
using google::scp::roma::sandbox::worker_api::ClearInputFields;
using google::scp::roma::sandbox::worker_api::CreateWorker;
using google::scp::roma::sandbox::worker_api::GetEngineOneTimeSetup;
using google::scp::roma::sandbox::worker_api::V8WorkerEngineParams;
using sandbox2::Buffer;

namespace {

// the pointer of the data shared sandbox2::Buffer which is used to share
// data between the host process and the sandboxee.
std::unique_ptr<Buffer> sandbox_data_shared_buffer_ptr_{nullptr};
size_t request_and_response_data_buffer_size_bytes_{0};

std::unique_ptr<Worker> worker_{nullptr};

SapiStatusCode Init(worker_api::WorkerInitParamsProto* init_params) {
  if (worker_) {
    SapiStatusCode status = Stop();
    // If we fail to stop the previous worker then log but keep going because
    // we'll be recreating it momentarily.
    if (status != SapiStatusCode::kOk) {
      ROMA_VLOG(1) << SapiStatusCodeToAbslStatus(
          static_cast<int>(status), "Failed to stop previous worker");
    }
  }

  std::vector<std::string> native_js_function_names(
      init_params->native_js_function_names().begin(),
      init_params->native_js_function_names().end());

  std::vector<std::string> rpc_method_names(
      init_params->rpc_method_names().begin(),
      init_params->rpc_method_names().end());

  std::vector<std::string> v8_flags(init_params->v8_flags().begin(),
                                    init_params->v8_flags().end());

  JsEngineResourceConstraints resource_constraints;
  resource_constraints.initial_heap_size_in_mb =
      static_cast<size_t>(init_params->js_engine_initial_heap_size_mb());
  resource_constraints.maximum_heap_size_in_mb =
      static_cast<size_t>(init_params->js_engine_maximum_heap_size_mb());

  V8WorkerEngineParams v8_params = {
      .native_js_function_comms_fd = init_params->native_js_function_comms_fd(),
      .native_js_function_names = std::move(native_js_function_names),
      .rpc_method_names = std::move(rpc_method_names),
      .server_address = init_params->server_address(),
      .resource_constraints = resource_constraints,
      .max_wasm_memory_number_of_pages = static_cast<size_t>(
          init_params->js_engine_max_wasm_memory_number_of_pages()),
      .require_preload = init_params->require_code_preload_for_execution(),
      .skip_v8_cleanup = init_params->skip_v8_cleanup(),
      .v8_flags = std::move(v8_flags),
      .enable_profilers = init_params->enable_profilers(),
      .logging_function_set = init_params->logging_function_set(),
      .disable_udf_stacktraces_in_response =
          init_params->disable_udf_stacktraces_in_response(),
  };

  worker_ = CreateWorker(v8_params);

  if (init_params->request_and_response_data_buffer_fd() == kBadFd) {
    return SapiStatusCode::kValidSandboxBufferRequired;
  }
  // create Buffer from file descriptor.
  auto buffer =
      Buffer::CreateFromFd(init_params->request_and_response_data_buffer_fd());
  if (!buffer.ok()) {
    return SapiStatusCode::kFailedToCreateBufferInsideSandboxee;
  }

  sandbox_data_shared_buffer_ptr_ = std::move(buffer).value();
  request_and_response_data_buffer_size_bytes_ =
      init_params->request_and_response_data_buffer_size_bytes();

  ROMA_VLOG(1) << "Worker wrapper successfully created the worker";
  return SapiStatusCode::kOk;
}

SapiStatusCode RunCode(worker_api::WorkerParamsProto* params) {
  if (!worker_) {
    return SapiStatusCode::kUninitializedWorker;
  }

  const auto& code = params->code();

  // WorkerParamsProto one of for `input_strings` or `input_bytes` or neither.
  std::vector<std::string_view> input;
  auto input_type = params->metadata().find(
      google::scp::roma::sandbox::constants::kInputType);
  if (input_type != params->metadata().end() &&
      input_type->second ==
          google::scp::roma::sandbox::constants::kInputTypeBytes) {
    input.push_back(params->input_bytes());
  } else {
    input.reserve(params->input_strings().inputs_size());
    for (int i = 0; i < params->input_strings().inputs_size(); i++) {
      input.push_back(params->input_strings().inputs().at(i));
    }
  }

  const absl::flat_hash_map<std::string_view, std::string_view> metadata(
      params->metadata().begin(), params->metadata().end());
  auto wasm_bin = reinterpret_cast<const uint8_t*>(params->wasm().c_str());
  absl::Span<const uint8_t> wasm =
      absl::MakeConstSpan(wasm_bin, params->wasm().length());

  privacy_sandbox::server_common::Stopwatch stopwatch;
  auto response_or = worker_->RunCode(code, input, metadata, wasm);
  auto js_duration = privacy_sandbox::server_common::EncodeGoogleApiProto(
      stopwatch.GetElapsedTime());
  if (!js_duration.ok()) {
    return SapiStatusCode::kInvalidDuration;
  }
  (*params->mutable_metrics())[kExecutionMetricJsEngineCallDuration] =
      std::move(js_duration).value();

  if (!response_or.ok()) {
    params->set_error_message(std::string(response_or.status().message()));
    return SapiStatusCode::kExecutionFailed;
  }

  for (const auto& pair : response_or.value().metrics) {
    auto duration =
        privacy_sandbox::server_common::EncodeGoogleApiProto(pair.second);
    if (!duration.ok()) {
      return SapiStatusCode::kInvalidDuration;
    }
    (*params->mutable_metrics())[pair.first] = std::move(duration).value();
  }

  params->set_response(std::move(response_or.value().response));
  params->set_profiler_output(std::move(response_or.value().profiler_output));
  return SapiStatusCode::kOk;
}

}  // namespace

SapiStatusCode InitFromSerializedData(sapi::LenValStruct* data) {
  worker_api::WorkerInitParamsProto init_params;
  if (!init_params.ParseFromArray(data->data, data->size)) {
    return SapiStatusCode::kCouldNotDeserializeInitData;
  }

  ROMA_VLOG(1) << "Worker wrapper successfully received the init data";
  return Init(&init_params);
}

SapiStatusCode Run() {
  if (!worker_) {
    return SapiStatusCode::kUninitializedWorker;
  }
  worker_->Run();
  return SapiStatusCode::kOk;
}

SapiStatusCode Stop() {
  if (!worker_) {
    return SapiStatusCode::kUninitializedWorker;
  }
  worker_->Stop();
  worker_.reset();
  return SapiStatusCode::kOk;
}

SapiStatusCode RunCodeFromSerializedData(sapi::LenValStruct* data,
                                         int input_serialized_size,
                                         size_t* output_serialized_size) {
  ROMA_VLOG(1)
      << "Worker wrapper RunCodeFromSerializedData() received the request";

  if (!sandbox_data_shared_buffer_ptr_) {
    return SapiStatusCode::kValidSandboxBufferRequired;
  }

  worker_api::WorkerParamsProto params;

  // If input_serialized_size is greater than zero, then the data is shared by
  // the buffer.
  if (input_serialized_size > 0) {
    if (!params.ParseFromArray(sandbox_data_shared_buffer_ptr_->data(),
                               input_serialized_size)) {
      LOG(ERROR) << "Could not deserialize run_code request from sandbox "
                    "buffer. The input_serialized_size in Bytes is "
                 << input_serialized_size;
      return SapiStatusCode::kCouldNotDeserializeRunData;
    }
  } else if (!params.ParseFromArray(data->data, data->size)) {
    LOG(ERROR) << "Could not deserialize run_code request from "
                  "sapi::LenValStruct* with data size "
               << data->size;
    return SapiStatusCode::kCouldNotDeserializeRunData;
  }

  const auto result = RunCode(&params);
  if (result != SapiStatusCode::kOk && params.error_message().empty()) {
    return result;
  }

  // Don't return the input or code.
  ClearInputFields(params);

  const size_t serialized_size = params.ByteSizeLong();
  if (serialized_size < request_and_response_data_buffer_size_bytes_) {
    ROMA_VLOG(1) << "Response data sharing with Buffer";

    // Write the response into Buffer.
    if (!params.SerializeToArray(sandbox_data_shared_buffer_ptr_->data(),
                                 serialized_size)) {
      LOG(ERROR) << "Failed to serialize run_code response into buffer with "
                    "serialized_size in Bytes "
                 << serialized_size;
      return SapiStatusCode::kCouldNotSerializeResponseData;
    }

    *output_serialized_size = serialized_size;

  } else {
    ROMA_VLOG(1) << "Response serialized size " << serialized_size
                 << "Bytes is larger than the Buffer capacity "
                 << request_and_response_data_buffer_size_bytes_
                 << "Bytes. Data sharing with Bytes";

    uint8_t* serialized_data = static_cast<uint8_t*>(malloc(serialized_size));
    if (!serialized_data) {
      LOG(ERROR) << "Failed to allocate uint8_t* serialized_data with size of "
                 << serialized_size;
      return SapiStatusCode::kCouldNotSerializeResponseData;
    }

    if (!params.SerializeToArray(serialized_data, serialized_size)) {
      LOG(ERROR) << "Failed to serialize run_code response into uint8_t* "
                    "serialized_data with serialized_size of "
                 << serialized_size;
      // In this failure scenario, free the buffer.
      // We don't free it otherwise, as it is free'd in the destructor of the
      // LenValStruct* passed to this function, which is owned by SAPI.
      free(serialized_data);
      return SapiStatusCode::kCouldNotSerializeResponseData;
    }

    // Free old data
    free(data->data);

    data->data = serialized_data;
    data->size = serialized_size;

    // output_serialized_size set to 0 to indicate the response shared by Bytes.
    *output_serialized_size = 0;
  }

  ROMA_VLOG(1) << "Worker wrapper successfully executed the request";
  return result;
}

SapiStatusCode RunCodeFromBuffer(int input_serialized_size,
                                 size_t* output_serialized_size) {
  ROMA_VLOG(1) << "Worker wrapper RunCodeFromBuffer() received the request";

  if (!sandbox_data_shared_buffer_ptr_) {
    return SapiStatusCode::kValidSandboxBufferRequired;
  }

  worker_api::WorkerParamsProto params;
  if (!params.ParseFromArray(sandbox_data_shared_buffer_ptr_->data(),
                             input_serialized_size)) {
    LOG(ERROR) << "Could not deserialize run_code request from sandbox";
    return SapiStatusCode::kCouldNotDeserializeRunData;
  }

  ROMA_VLOG(1) << "Worker wrapper successfully received the request data";
  const auto result = RunCode(&params);
  if (result != SapiStatusCode::kOk && params.error_message().empty()) {
    return result;
  }

  // Don't return the input or code.
  ClearInputFields(params);

  auto serialized_size = params.ByteSizeLong();
  if (serialized_size > request_and_response_data_buffer_size_bytes_) {
    LOG(ERROR) << "Serialized data size " << serialized_size
               << " Bytes is larger than Buffer capacity "
               << request_and_response_data_buffer_size_bytes_ << " Bytes.";
    return SapiStatusCode::kResponseLargerThanBuffer;
  }

  if (!params.SerializeToArray(sandbox_data_shared_buffer_ptr_->data(),
                               serialized_size)) {
    LOG(ERROR) << "Failed to serialize run_code response into buffer";
    return SapiStatusCode::kCouldNotSerializeResponseData;
  }

  *output_serialized_size = serialized_size;

  ROMA_VLOG(1) << "Worker wrapper successfully executed the request";
  return result;
}
