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

#ifndef ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_
#define ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "core/async_executor/src/async_executor.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"
#include "roma/sandbox/dispatcher/src/dispatcher.h"
#include "roma/sandbox/native_function_binding/src/native_function_handler_sapi_ipc.h"
#include "roma/sandbox/native_function_binding/src/native_function_table.h"
#include "roma/sandbox/worker_pool/src/worker_pool.h"

#include "error_codes.h"

using google::scp::core::errors::GetErrorMessage;

namespace google::scp::roma::sandbox::roma_service {
class RomaService {
 public:
  absl::Status Init();

  absl::Status LoadCodeObj(std::unique_ptr<CodeObject> code_object,
                           Callback callback);

  // Async API.
  // Execute single invocation request. Can only be called when a valid
  // code object has been loaded.
  absl::Status Execute(
      std::unique_ptr<InvocationRequestStrInput> invocation_request,
      Callback callback);

  absl::Status Execute(
      std::unique_ptr<InvocationRequestSharedInput> invocation_request,
      Callback callback);

  absl::Status Execute(
      std::unique_ptr<InvocationRequestStrViewInput> invocation_request,
      Callback callback);

  // Async & Batch API.
  // Batch execute a batch of invocation requests. Can only be called when a
  // valid code object has been loaded.
  absl::Status BatchExecute(std::vector<InvocationRequestStrInput>& batch,
                            BatchCallback batch_callback);

  absl::Status BatchExecute(std::vector<InvocationRequestSharedInput>& batch,
                            BatchCallback batch_callback);

  absl::Status BatchExecute(std::vector<InvocationRequestStrViewInput>& batch,
                            BatchCallback batch_callback);

  absl::Status Stop();

  RomaService(const RomaService&) = delete;

  explicit RomaService(const Config config = Config())
      : config_(std::move(config)) {}

 private:
  core::ExecutionResult InitInternal() noexcept;
  core::ExecutionResult RunInternal() noexcept;
  core::ExecutionResult StopInternal() noexcept;

  core::ExecutionResult RegisterMetadata(std::string uuid, TMetadata metadata);

  struct NativeFunctionBindingSetup {
    std::vector<int> remote_file_descriptors;
    std::vector<int> local_file_descriptors;
    std::vector<std::string> js_function_names;
  };

  /**
   * @brief Setup the handler, create the socket pairs and return the sockets
   * that belongs to the sandbox side.
   * @param concurrency The number of processes to create resources for.
   *
   * @return A struct containing the remote function binding information
   */
  core::ExecutionResultOr<NativeFunctionBindingSetup>
  SetupNativeFunctionHandler(size_t concurrency);

  void RegisterLogBindings() noexcept;

  core::ExecutionResult SetupWorkers(
      const NativeFunctionBindingSetup& native_binding_setup);

  template <typename RequestT>
  absl::Status ExecutionObjectValidation(const std::string& function_name,
                                         const RequestT& invocation_req) {
    if (invocation_req->version_num == 0) {
      return absl::Status(
          absl::StatusCode::kInvalidArgument,
          "Roma " + function_name + " failed due to invalid version.");
    }

    if (invocation_req->handler_name.empty()) {
      return absl::Status(
          absl::StatusCode::kInvalidArgument,
          "Roma " + function_name + " failed due to empty handler name.");
    }

    return absl::OkStatus();
  }

  template <typename RequestT>
  absl::Status ExecuteInternal(std::unique_ptr<RequestT> invocation_req,
                               Callback callback) {
    auto validation =
        ExecutionObjectValidation("Execute", invocation_req.get());
    if (!validation.ok()) {
      return validation;
    }

    auto request_unique_id = google::scp::core::common::Uuid::GenerateUuid();
    auto it_pair = invocation_req->tags.emplace(
        google::scp::roma::sandbox::constants::kRequestUuid,
        google::scp::core::common::ToString(request_unique_id));
    RegisterMetadata(it_pair.first->second, invocation_req->metadata);

    const auto result =
        dispatcher_->Dispatch(std::move(invocation_req), std::move(callback));
    if (!result.Successful()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Roma Execute failed due to: " +
                              std::string(GetErrorMessage(result.status_code)));
    }
    return absl::OkStatus();
  }

  template <typename RequestT>
  absl::Status BatchExecuteInternal(std::vector<RequestT>& batch,
                                    BatchCallback batch_callback) {
    for (auto& request : batch) {
      auto validation = ExecutionObjectValidation("BatchExecute", &request);
      if (!validation.ok()) {
        return validation;
      }
    }

    auto result = dispatcher_->DispatchBatch(batch, std::move(batch_callback));
    if (!result.Successful()) {
      return absl::Status(absl::StatusCode::kInternal,
                          "Roma Batch Execute failed due to dispatch error: " +
                              std::string(GetErrorMessage(result.status_code)));
    }
    return absl::OkStatus();
  }

  static absl::Mutex instance_mu_;
  static RomaService* instance_ ABSL_GUARDED_BY(instance_mu_);
  Config config_;
  std::unique_ptr<dispatcher::Dispatcher> dispatcher_;
  std::unique_ptr<worker_pool::WorkerPool> worker_pool_;
  std::unique_ptr<core::AsyncExecutor> async_executor_;
  native_function_binding::NativeFunctionTable native_function_binding_table_;
  std::shared_ptr<native_function_binding::NativeFunctionHandlerSapiIpc>
      native_function_binding_handler_;
};
}  // namespace google::scp::roma::sandbox::roma_service

#endif  // ROMA_SANDBOX_ROMA_SERVICE_SRC_ROMA_SERVICE_H_
