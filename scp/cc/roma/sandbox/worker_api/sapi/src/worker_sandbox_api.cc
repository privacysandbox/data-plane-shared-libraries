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

#include "worker_sandbox_api.h"

#include <stdint.h>
#include <sys/syscall.h>

#include <linux/audit.h>

#include <chrono>
#include <limits>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker_api/sapi/src/worker_init_params.pb.h"
#include "roma/sandbox/worker_api/sapi/src/worker_params.pb.h"
#include "sandboxed_api/lenval_core.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"

#include "error_codes.h"

#define ROMA_CONVERT_MB_TO_BYTES(mb) mb * 1024 * 1024

using absl::SimpleAtoi;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_API_COULD_NOT_CREATE_IPC_PROTO;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_GET_PROTO_MESSAGE_AFTER_EXECUTION;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_SANDBOX;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_WRAPPER_API;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_RUN_CODE_THROUGH_WRAPPER_API;
using google::scp::core::errors::SC_ROMA_WORKER_API_COULD_NOT_RUN_WRAPPER_API;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_INIT_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_BUFFER_FD_TO_SANDBOXEE;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_FUNCTION_FD_TO_SANDBOXEE;
using google::scp::core::errors::
    SC_ROMA_WORKER_API_REQUEST_DATA_SIZE_LARGER_THAN_BUFFER_CAPACITY;
using google::scp::core::errors::SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX;
using google::scp::core::errors::SC_ROMA_WORKER_API_WORKER_CRASHED;
using google::scp::roma::sandbox::constants::kBadFd;
using std::make_unique;
using std::move;
using std::numeric_limits;
using std::string;
using std::unique_ptr;
using std::vector;
using std::this_thread::yield;

namespace google::scp::roma::sandbox::worker_api {

void WorkerSandboxApi::CreateWorkerSapiSandbox() noexcept {
  // Get the environment variable ROMA_VLOG_LEVEL value.
  int external_verbose_level = logging::GetVlogVerboseLevel();

  worker_sapi_sandbox_ = make_unique<WorkerSapiSandbox>(
      ROMA_CONVERT_MB_TO_BYTES(max_worker_virtual_memory_mb_),
      external_verbose_level);
}

int WorkerSandboxApi::TransferFdAndGetRemoteFd(
    std::unique_ptr<::sapi::v::Fd> local_fd) noexcept {
  // We don't want the SAPI FD object to manage the local FD or it'd close it
  // upon its deletion.
  local_fd->OwnLocalFd(false);
  auto transferred = worker_sapi_sandbox_->TransferToSandboxee(local_fd.get());
  if (!transferred.ok()) {
    return kBadFd;
  }

  // This is to support recreating the SAPI FD object upon restarts, otherwise
  // destroying the object will try to close a non-existent file. And it has
  // to be done after the call to TransferToSandboxee.
  local_fd->OwnRemoteFd(false);
  return local_fd->GetRemoteFd();
}

ExecutionResult WorkerSandboxApi::Init() noexcept {
  if (worker_sapi_sandbox_) {
    worker_sapi_sandbox_->Terminate();
    // Wait for the sandbox to become INACTIVE
    while (worker_sapi_sandbox_->is_active()) {
      yield();
    }

    ROMA_VLOG(1) << "Successfully terminated the existing sapi sandbox";
  }

  CreateWorkerSapiSandbox();

  auto status = worker_sapi_sandbox_->Init();
  if (!status.ok()) {
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_SANDBOX);
  }

  worker_wrapper_api_ =
      make_unique<WorkerWrapperApi>(worker_sapi_sandbox_.get());

  // Wait for the sandbox to become ACTIVE
  while (!worker_sapi_sandbox_->is_active()) {
    yield();
  }
  ROMA_VLOG(1) << "the sapi sandbox is active";

  int js_hook_remote_fd = kBadFd;
  if (native_js_function_comms_fd_ != kBadFd) {
    js_hook_remote_fd = TransferFdAndGetRemoteFd(
        make_unique<::sapi::v::Fd>(native_js_function_comms_fd_));
    if (js_hook_remote_fd == kBadFd) {
      return FailureExecutionResult(
          SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_FUNCTION_FD_TO_SANDBOXEE);
    }

    ROMA_VLOG(2)
        << "successfully set up the remote_fd " << js_hook_remote_fd
        << " and local_fd " << native_js_function_comms_fd_
        << " for native JS hook function invocation from the sapi sandbox";
  }

  int buffer_remote_fd = TransferFdAndGetRemoteFd(
      make_unique<::sapi::v::Fd>(sandbox_data_shared_buffer_ptr_->fd()));
  if (buffer_remote_fd == kBadFd) {
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_TRANSFER_BUFFER_FD_TO_SANDBOXEE);
  }
  ROMA_VLOG(2) << "successfully set up the remote_fd " << buffer_remote_fd
               << " and local_fd " << sandbox_data_shared_buffer_ptr_->fd()
               << " for the buffer of the sapi sandbox";

  ::worker_api::WorkerInitParamsProto worker_init_params;
  worker_init_params.set_worker_factory_js_engine(
      static_cast<int>(worker_engine_));
  worker_init_params.set_require_code_preload_for_execution(require_preload_);
  worker_init_params.set_compilation_context_cache_size(
      compilation_context_cache_size_);
  worker_init_params.set_native_js_function_comms_fd(js_hook_remote_fd);
  worker_init_params.mutable_native_js_function_names()->Assign(
      native_js_function_names_.begin(), native_js_function_names_.end());
  worker_init_params.set_js_engine_initial_heap_size_mb(
      js_engine_initial_heap_size_mb_);
  worker_init_params.set_js_engine_maximum_heap_size_mb(
      js_engine_maximum_heap_size_mb_);
  worker_init_params.set_js_engine_max_wasm_memory_number_of_pages(
      js_engine_max_wasm_memory_number_of_pages_);
  worker_init_params.set_request_and_response_data_buffer_fd(buffer_remote_fd);
  worker_init_params.set_request_and_response_data_buffer_size_bytes(
      request_and_response_data_buffer_size_bytes_);

  const auto serialized_size = worker_init_params.ByteSizeLong();
  vector<char> serialized_data(serialized_size);
  if (!worker_init_params.SerializeToArray(serialized_data.data(),
                                           serialized_size)) {
    LOG(ERROR) << "Failed to serialize init data.";
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_INIT_DATA);
  }

  sapi::v::LenVal sapi_len_val(static_cast<const char*>(serialized_data.data()),
                               serialized_size);
  const auto status_or =
      worker_wrapper_api_->InitFromSerializedData(sapi_len_val.PtrBefore());
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to init the worker via the wrapper with: "
               << status_or.status().message();
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_INITIALIZE_WRAPPER_API);
  }
  if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  ROMA_VLOG(1) << "Successfully init the worker in the sapi sandbox";
  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::Run() noexcept {
  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return FailureExecutionResult(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX);
  }

  auto status_or = worker_wrapper_api_->Run();
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to run the worker via the wrapper with: "
               << status_or.status().message();
    return FailureExecutionResult(SC_ROMA_WORKER_API_COULD_NOT_RUN_WRAPPER_API);
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::Stop() noexcept {
  if ((!worker_sapi_sandbox_ && !worker_wrapper_api_) ||
      (worker_sapi_sandbox_ && !worker_sapi_sandbox_->is_active())) {
    // Nothing to stop, just return
    return SuccessExecutionResult();
  }

  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return FailureExecutionResult(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX);
  }

  auto status_or = worker_wrapper_api_->Stop();
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to stop the worker via the wrapper with: "
               << status_or.status().message();
    // The worker had already died so nothing to stop
    return SuccessExecutionResult();
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  worker_sapi_sandbox_->Terminate();

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::InternalRunCode(
    ::worker_api::WorkerParamsProto& params) noexcept {
  int serialized_size = params.ByteSizeLong();

  unique_ptr<sapi::v::LenVal> sapi_len_val;
  int input_serialized_size(serialized_size);
  sapi::v::IntBase<size_t> output_serialized_size_ptr;

  if (serialized_size < request_and_response_data_buffer_size_bytes_) {
    ROMA_VLOG(1) << "Request data sharing with Buffer";

    if (!params.SerializeToArray(sandbox_data_shared_buffer_ptr_->data(),
                                 serialized_size)) {
      LOG(ERROR) << "Failed to serialize run_code request into buffer. The "
                    "request's ByteSizeLong is "
                 << serialized_size;
      return FailureExecutionResult(
          SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA);
    }

    sapi_len_val = make_unique<sapi::v::LenVal>("", 0);
  } else {
    ROMA_VLOG(1) << "Request serialized size " << serialized_size
                 << "bytes is larger than the Buffer capacity "
                 << request_and_response_data_buffer_size_bytes_
                 << "bytes. Data sharing with sapi::v::LenVal Bytes";

    // Set input_serialized_size to 0 to indicate the data shared by LenVal.
    input_serialized_size = 0;
    vector<uint8_t> serialized_data(serialized_size);
    if (!params.SerializeToArray(serialized_data.data(), serialized_size)) {
      LOG(ERROR) << "Failed to serialize run_code request protobuf into array.";
      return FailureExecutionResult(
          SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA);
    }
    sapi_len_val = make_unique<sapi::v::LenVal>(serialized_data);
  }

  auto status_or = worker_wrapper_api_->RunCodeFromSerializedData(
      sapi_len_val->PtrBoth(), input_serialized_size,
      output_serialized_size_ptr.PtrAfter());

  if (!status_or.ok()) {
    return RetryExecutionResult(SC_ROMA_WORKER_API_WORKER_CRASHED);
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  ::worker_api::WorkerParamsProto out_params;
  if (output_serialized_size_ptr.GetValue() > 0) {
    if (!out_params.ParseFromArray(sandbox_data_shared_buffer_ptr_->data(),
                                   output_serialized_size_ptr.GetValue())) {
      LOG(ERROR) << "Could not deserialize run_code response from the Buffer. "
                    "The response serialized size in Bytes is "
                 << output_serialized_size_ptr.GetValue();
      return FailureExecutionResult(
          SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA);
    }
  } else if (!out_params.ParseFromArray(sapi_len_val->GetData(),
                                        sapi_len_val->GetDataSize())) {
    LOG(ERROR) << "Could not deserialize run_code response from "
                  "sapi::v::LenVal. The sapi::v::LenVal data size in Bytes is "
               << sapi_len_val->GetDataSize();
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA);
  }

  params = move(out_params);

  return SuccessExecutionResult();
}

core::ExecutionResult WorkerSandboxApi::InternalRunCodeBufferShareOnly(
    ::worker_api::WorkerParamsProto& params) noexcept {
  int serialized_size = params.ByteSizeLong();
  if (serialized_size > request_and_response_data_buffer_size_bytes_) {
    LOG(ERROR) << "Request serialized size in Bytes " << serialized_size
               << " is larger than the Buffer capacity in Bytes "
               << request_and_response_data_buffer_size_bytes_;
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_REQUEST_DATA_SIZE_LARGER_THAN_BUFFER_CAPACITY);
  }

  if (!params.SerializeToArray(sandbox_data_shared_buffer_ptr_->data(),
                               serialized_size)) {
    LOG(ERROR) << "Failed to serialize run_code request into buffer. The "
                  "request serialized size in Bytes is "
               << serialized_size;
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_SERIALIZE_RUN_CODE_DATA);
  }

  sapi::v::IntBase<size_t> output_serialized_size_ptr;
  auto status_or = worker_wrapper_api_->RunCodeFromBuffer(
      serialized_size, output_serialized_size_ptr.PtrAfter());

  if (!status_or.ok()) {
    return RetryExecutionResult(SC_ROMA_WORKER_API_WORKER_CRASHED);
  } else if (*status_or != SC_OK) {
    return FailureExecutionResult(*status_or);
  }

  ::worker_api::WorkerParamsProto out_params;
  if (!out_params.ParseFromArray(sandbox_data_shared_buffer_ptr_->data(),
                                 output_serialized_size_ptr.GetValue())) {
    LOG(ERROR) << "Could not deserialize run_code response from sandboxee. The "
                  "response serialized size in Bytes is "
               << output_serialized_size_ptr.GetValue();
    return FailureExecutionResult(
        SC_ROMA_WORKER_API_COULD_NOT_DESERIALIZE_RUN_CODE_DATA);
  }

  params = move(out_params);

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::RunCode(
    ::worker_api::WorkerParamsProto& params) noexcept {
  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return FailureExecutionResult(SC_ROMA_WORKER_API_UNINITIALIZED_SANDBOX);
  }

  ExecutionResult run_code_result;
  if (enable_sandbox_sharing_request_response_with_buffer_only_) {
    run_code_result = InternalRunCodeBufferShareOnly(params);
  } else {
    run_code_result = InternalRunCode(params);
  }

  if (!run_code_result.Successful()) {
    if (run_code_result.Retryable()) {
      // This means that the sandbox died so we need to restart it.
      auto result = Init();
      RETURN_IF_FAILURE(result);
      result = Run();
      RETURN_IF_FAILURE(result);
    }

    return run_code_result;
  }

  return SuccessExecutionResult();
}

ExecutionResult WorkerSandboxApi::Terminate() noexcept {
  worker_sapi_sandbox_->Terminate();
  return SuccessExecutionResult();
}
}  // namespace google::scp::roma::sandbox::worker_api
