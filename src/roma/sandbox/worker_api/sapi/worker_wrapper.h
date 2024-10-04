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

#ifndef ROMA_SANDBOX_WORKER_API_SAPI_WORKER_WRAPPER_H_
#define ROMA_SANDBOX_WORKER_API_SAPI_WORKER_WRAPPER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sandboxed_api/sandbox2/buffer.h"
#include "src/roma/sandbox/worker/worker.h"
#include "src/roma/sandbox/worker_api/sapi/error_codes.h"
#include "src/roma/sandbox/worker_api/sapi/worker_init_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_sapi_sandbox.h"
#include "src/roma/sandbox/worker_api/sapi/worker_wrapper-sapi.sapi.h"

namespace google::scp::roma::sandbox::worker_api {

class WorkerWrapper {
 public:
  WorkerWrapper(bool enable_sandbox_sharing_request_response_with_buffer_only,
                size_t request_and_response_data_buffer_size_bytes,
                sandbox2::Buffer* sandbox_data_shared_buffer_ptr,
                int native_js_function_comms_fd,
                size_t max_worker_virtual_memory_mb)
      : enable_sandbox_sharing_request_response_with_buffer_only_(
            enable_sandbox_sharing_request_response_with_buffer_only),
        request_and_response_data_buffer_size_bytes_(
            request_and_response_data_buffer_size_bytes),
        sandbox_data_shared_buffer_ptr_(sandbox_data_shared_buffer_ptr),
        native_js_function_comms_fd_(native_js_function_comms_fd),
        max_worker_virtual_memory_mb_(max_worker_virtual_memory_mb) {}

  absl::Status Init(::worker_api::WorkerInitParamsProto& init_params);

  absl::Status Run();

  absl::Status Stop();

  std::pair<absl::Status, RetryStatus> RunCode(
      ::worker_api::WorkerParamsProto& params);

  void Terminate();

 protected:
  void WarmUpSandbox();

  bool SandboxIsInitialized();

  absl::Status CreateWorkerSapiSandbox();

  int TransferFdAndGetRemoteFd(std::unique_ptr<::sapi::v::Fd> local_fd);

  absl::Status TransferFds();

  std::pair<absl::Status, RetryStatus> InternalRunCode(
      ::worker_api::WorkerParamsProto& params);

  std::pair<absl::Status, RetryStatus> InternalRunCodeBufferShareOnly(
      ::worker_api::WorkerParamsProto& params);

  std::unique_ptr<google::scp::roma::sandbox::worker::Worker> worker_;
  const bool enable_sandbox_sharing_request_response_with_buffer_only_;
  size_t request_and_response_data_buffer_size_bytes_;
  sandbox2::Buffer* sandbox_data_shared_buffer_ptr_;
  // See BUILD file for named library "WorkerWrapper" in the
  // sapi_library worker_wrapper-sapi target.
  std::unique_ptr<WorkerWrapperApi> worker_wrapper_sapi_;
  int native_js_function_comms_fd_;
  size_t max_worker_virtual_memory_mb_;

  std::unique_ptr<WorkerSapiSandbox> worker_sapi_sandbox_;
  ::worker_api::WorkerInitParamsProto init_params_;
};

}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_WORKER_WRAPPER_H_
