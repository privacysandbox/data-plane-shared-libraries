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
#include "src/roma/sandbox/native_function_binding/native_function_invoker_sapi_ipc.h"
#include "src/roma/sandbox/worker/worker.h"
#include "src/roma/sandbox/worker_api/sapi/utils.h"
#include "src/roma/sandbox/worker_api/sapi/worker_init_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_wrapper-sapi.sapi.h"
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
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionInvokerSapiIpc;
using google::scp::roma::sandbox::worker::Worker;

namespace google::scp::roma::sandbox::worker_api {

namespace {
constexpr std::string_view kWarmupCode = " ";
constexpr std::string_view kWarmupRequestId = "warmup";
constexpr std::string_view kWarmupCodeVersion = "vWarmup";
}  // namespace

absl::Status WorkerWrapper::Init(
    ::worker_api::WorkerInitParamsProto& init_params) {
  std::string serialized_data = init_params.SerializeAsString();
  if (serialized_data.empty()) {
    LOG(ERROR) << "Failed to serialize init data.";
    return absl::InvalidArgumentError("Failed to serialize init data.");
  }
  sapi::v::LenVal sapi_len_val(serialized_data.data(), serialized_data.size());

  const auto worker_status =
      worker_wrapper_sapi_->InitFromSerializedData(sapi_len_val.PtrBefore());
  if (!worker_status.ok()) {
    return worker_status.status();
  }
  if (*worker_status != SapiStatusCode::kOk) {
    return SapiStatusCodeToAbslStatus(static_cast<int>(*worker_status));
  }
  return absl::OkStatus();
}

bool WorkerWrapper::SandboxIsInitialized() {
  return worker_wrapper_sapi_ != nullptr;
}

void WorkerWrapper::WarmUpSandbox() {
  using google::scp::roma::sandbox::constants::kCodeVersion;
  using google::scp::roma::sandbox::constants::kRequestAction;
  using google::scp::roma::sandbox::constants::kRequestActionLoad;
  using google::scp::roma::sandbox::constants::kRequestId;
  using google::scp::roma::sandbox::constants::kRequestType;
  using google::scp::roma::sandbox::constants::kRequestTypeJavascript;

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(kWarmupCode);
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kCodeVersion] = kWarmupCodeVersion;
  (*params_proto.mutable_metadata())[kRequestId] = kWarmupRequestId;
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionLoad;
  (void)RunCode(params_proto);
}

std::pair<absl::Status, RetryStatus> WorkerWrapper::InternalRunCode(
    ::worker_api::WorkerParamsProto& params) {
  const int serialized_size = params.ByteSizeLong();
  std::unique_ptr<sapi::v::LenVal> sapi_len_val;
  std::string len_val_data;
  int input_serialized_size = serialized_size;

  if (serialized_size < request_and_response_data_buffer_size_bytes_) {
    ROMA_VLOG(1) << "Request data sharing with Buffer";

    if (!params.SerializeToArray(sandbox_data_shared_buffer_ptr_->data(),
                                 serialized_size)) {
      LOG(ERROR) << "Failed to serialize run_code request into buffer. The "
                    "request's ByteSizeLong is "
                 << serialized_size;
      return WrapResultWithNoRetry(
          absl::InvalidArgumentError("Failed to serialize run_code data."));
    }
    sapi_len_val = std::make_unique<sapi::v::LenVal>(nullptr, 0);
  } else {
    ROMA_VLOG(1) << "Request serialized size " << serialized_size
                 << " bytes is larger than the Buffer capacity "
                 << request_and_response_data_buffer_size_bytes_
                 << " bytes. Data sharing with sapi::v::LenVal Bytes";

    // Set input_serialized_size to 0 to indicate the data shared by LenVal.
    input_serialized_size = 0;
    len_val_data.resize(serialized_size);
    if (!params.SerializeToString(&len_val_data)) {
      LOG(ERROR) << "Failed to serialize run_code request protobuf into array.";
      return WrapResultWithNoRetry(
          absl::InvalidArgumentError("Failed to serialize run_code data."));
    }
    sapi_len_val = std::make_unique<sapi::v::LenVal>(len_val_data.data(),
                                                     len_val_data.size());
  }

  sapi::v::IntBase<size_t> output_serialized_size_ptr;
  auto worker_status = worker_wrapper_sapi_->RunCodeFromSerializedData(
      sapi_len_val->PtrBoth(), input_serialized_size,
      output_serialized_size_ptr.PtrAfter());

  if (!worker_status.ok()) {
    std::string err_msg = "Sandbox worker crashed during execution of request.";
    return WrapResultWithRetry(absl::InternalError(err_msg));
  } else if (*worker_status != SapiStatusCode::kOk &&
             // If execution failed then the output may contain forwardable
             // error message.
             *worker_status != SapiStatusCode::kExecutionFailed) {
    return WrapResultWithNoRetry(
        SapiStatusCodeToAbslStatus(static_cast<int>(*worker_status)));
  }

  ::worker_api::WorkerParamsProto out_params;
  if (output_serialized_size_ptr.GetValue() > 0) {
    if (!out_params.ParseFromArray(sandbox_data_shared_buffer_ptr_->data(),
                                   output_serialized_size_ptr.GetValue())) {
      LOG(ERROR) << "Could not deserialize run_code response from the Buffer. "
                    "The response serialized size in Bytes is "
                 << output_serialized_size_ptr.GetValue();
      return WrapResultWithNoRetry(
          absl::InternalError("Failed to deserialize run_code data."));
    }
  } else if (!out_params.ParseFromArray(sapi_len_val->GetData(),
                                        sapi_len_val->GetDataSize())) {
    LOG(ERROR) << "Could not deserialize run_code response from "
                  "sapi::v::LenVal. The sapi::v::LenVal data size in Bytes is "
               << sapi_len_val->GetDataSize();
    return WrapResultWithNoRetry(
        absl::InternalError("Failed to deserialize run_code data."));
  }

  params = std::move(out_params);

  if (*worker_status != SapiStatusCode::kOk) {
    return WrapResultWithNoRetry(SapiStatusCodeToAbslStatus(
        static_cast<int>(*worker_status), params.error_message()));
  }
  return WrapResultWithNoRetry(absl::OkStatus());
}

std::pair<absl::Status, RetryStatus>
WorkerWrapper::InternalRunCodeBufferShareOnly(
    ::worker_api::WorkerParamsProto& params) {
  const int serialized_size = params.ByteSizeLong();
  if (serialized_size > request_and_response_data_buffer_size_bytes_) {
    LOG(ERROR) << "Request serialized size in Bytes " << serialized_size
               << " is larger than the Buffer capacity in Bytes "
               << request_and_response_data_buffer_size_bytes_;
    return WrapResultWithNoRetry(
        absl::ResourceExhaustedError("The size of request serialized data is "
                                     "larger than the Buffer capacity."));
  }

  if (!params.SerializeToArray(sandbox_data_shared_buffer_ptr_->data(),
                               serialized_size)) {
    LOG(ERROR) << "Failed to serialize run_code request into buffer. The "
                  "request serialized size in Bytes is "
               << serialized_size;
    return WrapResultWithNoRetry(
        absl::InvalidArgumentError("Failed to serialize run_code data."));
  }

  sapi::v::IntBase<size_t> output_serialized_size_ptr;
  auto worker_status = worker_wrapper_sapi_->RunCodeFromBuffer(
      serialized_size, output_serialized_size_ptr.PtrAfter());
  if (!worker_status.ok()) {
    return WrapResultWithRetry(absl::InternalError(
        "Sandbox worker crashed during execution of request."));
  } else if (*worker_status != SapiStatusCode::kOk &&
             // If execution failed then the output may contain forwardable
             // error message.
             *worker_status != SapiStatusCode::kExecutionFailed) {
    return WrapResultWithNoRetry(
        SapiStatusCodeToAbslStatus(static_cast<int>(*worker_status)));
  }

  ::worker_api::WorkerParamsProto out_params;
  if (!out_params.ParseFromArray(sandbox_data_shared_buffer_ptr_->data(),
                                 output_serialized_size_ptr.GetValue())) {
    LOG(ERROR) << "Could not deserialize run_code response from sandboxee. The "
                  "response serialized size in Bytes is "
               << output_serialized_size_ptr.GetValue();
    return WrapResultWithNoRetry(
        absl::InternalError("Failed to deserialize run_code data."));
  }

  params = std::move(out_params);

  if (*worker_status != SapiStatusCode::kOk) {
    return WrapResultWithNoRetry(SapiStatusCodeToAbslStatus(
        static_cast<int>(*worker_status), params.error_message()));
  }
  return WrapResultWithNoRetry(absl::OkStatus());
}

absl::Status WorkerWrapper::Run() {
  const auto worker_status = worker_wrapper_sapi_->Run();
  if (!worker_status.ok()) {
    return worker_status.status();
  }
  if (*worker_status != SapiStatusCode::kOk) {
    return SapiStatusCodeToAbslStatus(static_cast<int>(*worker_status));
  }
  WarmUpSandbox();
  return absl::OkStatus();
}

absl::Status WorkerWrapper::Stop() {
  const auto worker_status = worker_wrapper_sapi_->Stop();
  if (!worker_status.ok()) {
    // The worker had already died so nothing to stop
    return absl::OkStatus();
  } else if (*worker_status != SapiStatusCode::kOk) {
    return SapiStatusCodeToAbslStatus(static_cast<int>(*worker_status));
  }
  return absl::OkStatus();
}

std::pair<absl::Status, RetryStatus> WorkerWrapper::RunCode(
    ::worker_api::WorkerParamsProto& params) {
  ROMA_VLOG(1)
      << "Worker wrapper RunCodeFromSerializedData() received the request"
      << std::endl;
  std::pair<absl::Status, RetryStatus> run_code_result;
  if (enable_sandbox_sharing_request_response_with_buffer_only_) {
    run_code_result = InternalRunCodeBufferShareOnly(params);
  } else {
    run_code_result = InternalRunCode(params);
  }
  return run_code_result;
}

}  // namespace google::scp::roma::sandbox::worker_api
