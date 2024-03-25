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

#include "sandboxed_api/lenval_core.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/worker_api/sapi/error_codes.h"
#include "src/roma/sandbox/worker_api/sapi/worker_init_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

#define ROMA_CONVERT_MB_TO_BYTES(mb) mb * 1024 * 1024

using google::scp::roma::sandbox::constants::kBadFd;

namespace google::scp::roma::sandbox::worker_api {

namespace {

std::pair<absl::Status, WorkerSandboxApi::RetryStatus> WrapResultWithNoRetry(
    absl::Status result) {
  return std::make_pair(std::move(result),
                        WorkerSandboxApi::RetryStatus::kDoNotRetry);
}

std::pair<absl::Status, WorkerSandboxApi::RetryStatus> WrapResultWithRetry(
    absl::Status result) {
  return std::make_pair(std::move(result),
                        WorkerSandboxApi::RetryStatus::kRetry);
}

}  // namespace

WorkerSandboxApi::WorkerSandboxApi(
    bool require_preload, int native_js_function_comms_fd,
    const std::vector<std::string>& native_js_function_names,
    const std::vector<std::string>& rpc_method_names,
    const std::string& server_address, size_t max_worker_virtual_memory_mb,
    size_t js_engine_initial_heap_size_mb,
    size_t js_engine_maximum_heap_size_mb,
    size_t js_engine_max_wasm_memory_number_of_pages,
    size_t sandbox_request_response_shared_buffer_size_mb,
    bool enable_sandbox_sharing_request_response_with_buffer_only)
    : require_preload_(require_preload),
      native_js_function_comms_fd_(native_js_function_comms_fd),
      native_js_function_names_(native_js_function_names),
      rpc_method_names_(rpc_method_names),
      server_address_(server_address),
      max_worker_virtual_memory_mb_(max_worker_virtual_memory_mb),
      js_engine_initial_heap_size_mb_(js_engine_initial_heap_size_mb),
      js_engine_maximum_heap_size_mb_(js_engine_maximum_heap_size_mb),
      js_engine_max_wasm_memory_number_of_pages_(
          js_engine_max_wasm_memory_number_of_pages),
      enable_sandbox_sharing_request_response_with_buffer_only_(
          enable_sandbox_sharing_request_response_with_buffer_only) {
  // create a sandbox2 buffer
  request_and_response_data_buffer_size_bytes_ =
      sandbox_request_response_shared_buffer_size_mb > 0
          ? sandbox_request_response_shared_buffer_size_mb * kMB
          : kDefaultBufferSizeInMb * kMB;
  auto buffer = sandbox2::Buffer::CreateWithSize(
      request_and_response_data_buffer_size_bytes_);
  CHECK(buffer.ok()) << "Create Buffer with size failed with "
                     << buffer.status().message();
  sandbox_data_shared_buffer_ptr_ = std::move(buffer).value();
}

void WorkerSandboxApi::CreateWorkerSapiSandbox() {
  // Get the environment variable ROMA_VLOG_LEVEL value.
  int external_verbose_level = logging::GetVlogVerboseLevel();

  worker_sapi_sandbox_ = std::make_unique<WorkerSapiSandbox>(
      ROMA_CONVERT_MB_TO_BYTES(max_worker_virtual_memory_mb_),
      external_verbose_level);
}

int WorkerSandboxApi::TransferFdAndGetRemoteFd(
    std::unique_ptr<::sapi::v::Fd> local_fd) {
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

absl::Status WorkerSandboxApi::Init() {
  if (worker_sapi_sandbox_) {
    worker_sapi_sandbox_->Terminate(/*attempt_graceful_exit=*/false);
    ROMA_VLOG(1) << "Successfully terminated the existing sapi sandbox";
  }

  CreateWorkerSapiSandbox();

  auto status = worker_sapi_sandbox_->Init();
  if (!status.ok()) {
    return status;
  }

  worker_wrapper_api_ =
      std::make_unique<WorkerWrapperApi>(worker_sapi_sandbox_.get());

  int js_hook_remote_fd = kBadFd;
  if (native_js_function_comms_fd_ != kBadFd) {
    js_hook_remote_fd = TransferFdAndGetRemoteFd(
        std::make_unique<::sapi::v::Fd>(native_js_function_comms_fd_));
    if (js_hook_remote_fd == kBadFd) {
      return absl::InternalError(
          "Could not transfer function comms fd to sandboxee.");
    }

    ROMA_VLOG(2)
        << "successfully set up the remote_fd " << js_hook_remote_fd
        << " and local_fd " << native_js_function_comms_fd_
        << " for native JS hook function invocation from the sapi sandbox";
  }

  int buffer_remote_fd = TransferFdAndGetRemoteFd(
      std::make_unique<::sapi::v::Fd>(sandbox_data_shared_buffer_ptr_->fd()));
  if (buffer_remote_fd == kBadFd) {
    return absl::InternalError(
        "Could not transfer sandbox2::Buffer fd to sandboxee.");
  }
  ROMA_VLOG(2) << "successfully set up the remote_fd " << buffer_remote_fd
               << " and local_fd " << sandbox_data_shared_buffer_ptr_->fd()
               << " for the buffer of the sapi sandbox";

  ::worker_api::WorkerInitParamsProto worker_init_params;
  worker_init_params.set_require_code_preload_for_execution(require_preload_);
  worker_init_params.set_native_js_function_comms_fd(js_hook_remote_fd);
  worker_init_params.mutable_native_js_function_names()->Assign(
      native_js_function_names_.begin(), native_js_function_names_.end());
  worker_init_params.mutable_rpc_method_names()->Assign(
      rpc_method_names_.begin(), rpc_method_names_.end());
  worker_init_params.set_server_address(server_address_);
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
  std::vector<char> serialized_data(serialized_size);
  if (!worker_init_params.SerializeToArray(serialized_data.data(),
                                           serialized_size)) {
    LOG(ERROR) << "Failed to serialize init data.";
    return absl::InvalidArgumentError("Failed to serialize init data.");
  }

  sapi::v::LenVal sapi_len_val(static_cast<const char*>(serialized_data.data()),
                               serialized_size);
  const auto status_or =
      worker_wrapper_api_->InitFromSerializedData(sapi_len_val.PtrBefore());
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to init the worker via the wrapper with: "
               << status_or.status().message();
    return status_or.status();
  }
  if (*status_or != SapiStatusCode::kOk) {
    return SapiStatusCodeToAbslStatus(static_cast<int>(*status_or));
  }

  ROMA_VLOG(1) << "Successfully init the worker in the sapi sandbox";
  return absl::OkStatus();
}

absl::Status WorkerSandboxApi::Run() {
  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return absl::FailedPreconditionError(
        "Attempt to call API function with an uninitialized sandbox.");
  }

  auto status_or = worker_wrapper_api_->Run();
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to run the worker via the wrapper with: "
               << status_or.status().message();
    return status_or.status();
  } else if (*status_or != SapiStatusCode::kOk) {
    return SapiStatusCodeToAbslStatus(static_cast<int>(*status_or));
  }

  return absl::OkStatus();
}

absl::Status WorkerSandboxApi::Stop() {
  if ((!worker_sapi_sandbox_ && !worker_wrapper_api_) ||
      (worker_sapi_sandbox_ && !worker_sapi_sandbox_->is_active())) {
    // Nothing to stop, just return
    return absl::OkStatus();
  }

  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return absl::FailedPreconditionError(
        "Attempt to call API function with an uninitialized sandbox.");
  }

  auto status_or = worker_wrapper_api_->Stop();
  if (!status_or.ok()) {
    LOG(ERROR) << "Failed to stop the worker via the wrapper with: "
               << status_or.status().message();
    // The worker had already died so nothing to stop
    return absl::OkStatus();
  } else if (*status_or != SapiStatusCode::kOk) {
    return SapiStatusCodeToAbslStatus(static_cast<int>(*status_or));
  }

  worker_sapi_sandbox_->Terminate(/*attempt_graceful_exit=*/false);

  return absl::OkStatus();
}

std::pair<absl::Status, WorkerSandboxApi::RetryStatus>
WorkerSandboxApi::InternalRunCode(::worker_api::WorkerParamsProto& params) {
  const int serialized_size = params.ByteSizeLong();

  std::unique_ptr<sapi::v::LenVal> sapi_len_val;
  std::string len_val_data;
  int input_serialized_size(serialized_size);
  sapi::v::IntBase<size_t> output_serialized_size_ptr;

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
                 << "bytes is larger than the Buffer capacity "
                 << request_and_response_data_buffer_size_bytes_
                 << "bytes. Data sharing with sapi::v::LenVal Bytes";

    // Set input_serialized_size to 0 to indicate the data shared by LenVal.
    input_serialized_size = 0;
    len_val_data.resize(serialized_size);
    if (!params.SerializeToArray(len_val_data.data(), serialized_size)) {
      LOG(ERROR) << "Failed to serialize run_code request protobuf into array.";
      return WrapResultWithNoRetry(
          absl::InvalidArgumentError("Failed to serialize run_code data."));
    }
    sapi_len_val = std::make_unique<sapi::v::LenVal>(len_val_data.data(),
                                                     len_val_data.size());
  }

  auto status_or = worker_wrapper_api_->RunCodeFromSerializedData(
      sapi_len_val->PtrBoth(), input_serialized_size,
      output_serialized_size_ptr.PtrAfter());

  if (!status_or.ok()) {
    return WrapResultWithRetry(absl::InternalError(
        "Sandbox worker crashed during execution of request."));
  } else if (*status_or != SapiStatusCode::kOk) {
    return WrapResultWithNoRetry(
        SapiStatusCodeToAbslStatus(static_cast<int>(*status_or)));
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

  return WrapResultWithNoRetry(absl::OkStatus());
}

std::pair<absl::Status, WorkerSandboxApi::RetryStatus>
WorkerSandboxApi::InternalRunCodeBufferShareOnly(
    ::worker_api::WorkerParamsProto& params) {
  int serialized_size = params.ByteSizeLong();
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
  auto status_or = worker_wrapper_api_->RunCodeFromBuffer(
      serialized_size, output_serialized_size_ptr.PtrAfter());

  if (!status_or.ok()) {
    return WrapResultWithRetry(absl::InternalError(
        "Sandbox worker crashed during execution of request."));
  } else if (*status_or != SapiStatusCode::kOk) {
    return WrapResultWithNoRetry(
        SapiStatusCodeToAbslStatus(static_cast<int>(*status_or)));
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

  return WrapResultWithNoRetry(absl::OkStatus());
}

std::pair<absl::Status, WorkerSandboxApi::RetryStatus>
WorkerSandboxApi::RunCode(::worker_api::WorkerParamsProto& params) {
  if (!worker_sapi_sandbox_ || !worker_wrapper_api_) {
    return WrapResultWithNoRetry(absl::FailedPreconditionError(
        "Attempt to call API function with an uninitialized sandbox."));
  }

  std::pair<absl::Status, WorkerSandboxApi::RetryStatus> run_code_result;
  if (enable_sandbox_sharing_request_response_with_buffer_only_) {
    run_code_result = InternalRunCodeBufferShareOnly(params);
  } else {
    run_code_result = InternalRunCode(params);
  }

  if (!run_code_result.first.ok()) {
    if (run_code_result.second == WorkerSandboxApi::RetryStatus::kRetry) {
      // This means that the sandbox died so we need to restart it.
      if (auto status = Init(); !status.ok()) {
        return WrapResultWithNoRetry(status);
      }
      if (auto status = Run(); !status.ok()) {
        return WrapResultWithNoRetry(status);
      }
    }

    return run_code_result;
  }

  return WrapResultWithNoRetry(absl::OkStatus());
}

void WorkerSandboxApi::Terminate() { worker_sapi_sandbox_->Terminate(); }
}  // namespace google::scp::roma::sandbox::worker_api
