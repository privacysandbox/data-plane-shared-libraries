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
#include "src/roma/sandbox/worker_api/sapi/utils.h"
#include "src/roma/sandbox/worker_api/sapi/worker_init_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

#define ROMA_CONVERT_MB_TO_BYTES(mb) mb * 1024 * 1024

using google::scp::roma::sandbox::constants::kBadFd;

namespace google::scp::roma::sandbox::worker_api {

WorkerSandboxApi::WorkerSandboxApi(
    bool require_preload, int native_js_function_comms_fd,
    const std::vector<std::string>& native_js_function_names,
    const std::vector<std::string>& rpc_method_names,
    const std::string& server_address, size_t max_worker_virtual_memory_mb,
    size_t js_engine_initial_heap_size_mb,
    size_t js_engine_maximum_heap_size_mb,
    size_t js_engine_max_wasm_memory_number_of_pages,
    size_t sandbox_request_response_shared_buffer_size_mb,
    bool enable_sandbox_sharing_request_response_with_buffer_only,
    const std::vector<std::string>& v8_flags, bool enable_profilers,
    bool logging_function_set)
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
          enable_sandbox_sharing_request_response_with_buffer_only),
      v8_flags_(v8_flags),
      enable_profilers_(enable_profilers),
      logging_function_set_(logging_function_set) {
  // create a sandbox2 buffer
  request_and_response_data_buffer_size_bytes_ =
      sandbox_request_response_shared_buffer_size_mb > 0
          ? sandbox_request_response_shared_buffer_size_mb * kMB
          : kDefaultBufferSizeInMb * kMB;
  auto buffer = sandbox2::Buffer::CreateWithSize(
      request_and_response_data_buffer_size_bytes_);
  CHECK_OK(buffer) << "Create Buffer with size failed with "
                   << buffer.status().message();
  sandbox_data_shared_buffer_ptr_ = std::move(buffer).value();
}

// TODO: b/354929154 - Create SAPI sandbox only in non_prod mode
void WorkerSandboxApi::CreateWorkerSapiSandbox() {
  // Get the environment variable ROMA_VLOG_LEVEL value.
  const int external_verbose_level = logging::GetVlogVerboseLevel();
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

  worker_wrapper_ = std::make_unique<WorkerWrapper>(
      enable_sandbox_sharing_request_response_with_buffer_only_,
      request_and_response_data_buffer_size_bytes_,
      sandbox_data_shared_buffer_ptr_.get(), worker_sapi_sandbox_.get());

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
  worker_init_params.mutable_v8_flags()->Assign(v8_flags_.begin(),
                                                v8_flags_.end());
  worker_init_params.set_enable_profilers(enable_profilers_);
  worker_init_params.set_logging_function_set(logging_function_set_);

  const auto worker_status = worker_wrapper_->Init(worker_init_params);
  if (!worker_status.ok()) {
    LOG(ERROR) << "Failed to init the worker via the wrapper with: "
               << worker_status.message();
    return worker_status;
  }
  ROMA_VLOG(1) << "Successfully init the worker in the sapi sandbox";
  return absl::OkStatus();
}

bool WorkerSandboxApi::SandboxIsInitialized() {
  return worker_wrapper_ && worker_wrapper_->SandboxIsInitialized();
}

absl::Status WorkerSandboxApi::Run() {
  if (!SandboxIsInitialized()) {
    return absl::FailedPreconditionError(
        "Attempt to call API function with an uninitialized sandbox.");
  }

  const auto worker_status = worker_wrapper_->Run();
  if (!worker_status.ok()) {
    LOG(ERROR) << "Failed to run the worker via the wrapper with: "
               << worker_status.message();
    return worker_status;
  }
  return absl::OkStatus();
}

absl::Status WorkerSandboxApi::Stop() {
  if (!SandboxIsInitialized() ||
      (worker_sapi_sandbox_ && !worker_sapi_sandbox_->is_active())) {
    // Nothing to stop, just return
    return absl::OkStatus();
  }

  if (!SandboxIsInitialized()) {
    return absl::FailedPreconditionError(
        "Attempt to call API function with an uninitialized sandbox.");
  }

  const auto worker_status = worker_wrapper_->Stop();
  if (!worker_status.ok()) {
    LOG(ERROR) << "Failed to stop the worker via the wrapper with: "
               << worker_status.message();
    // The worker had already died so nothing to stop
    return absl::OkStatus();
  }

  worker_sapi_sandbox_->Terminate(/*attempt_graceful_exit=*/false);
  return absl::OkStatus();
}

std::pair<absl::Status, RetryStatus> WorkerSandboxApi::RunCode(
    ::worker_api::WorkerParamsProto& params) {
  if (!SandboxIsInitialized()) {
    return WrapResultWithNoRetry(absl::FailedPreconditionError(
        "Attempt to call API function with an uninitialized sandbox."));
  }
  std::pair<absl::Status, RetryStatus> run_code_result =
      worker_wrapper_->RunCode(params);
  if (!run_code_result.first.ok()) {
    if (run_code_result.second == RetryStatus::kRetry) {
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
