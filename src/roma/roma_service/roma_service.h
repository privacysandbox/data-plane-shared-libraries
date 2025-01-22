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

#ifndef ROMA_SANDBOX_ROMA_SERVICE_ROMA_SERVICE_H_
#define ROMA_SANDBOX_ROMA_SERVICE_ROMA_SERVICE_H_

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "src/core/os/linux/system_resource_info_provider_linux.h"
#include "src/roma/logging/logging.h"
#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/roma/native_function_grpc_server/native_function_grpc_server.h"
#include "src/roma/native_function_grpc_server/request_handlers.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/dispatcher/dispatcher.h"
#include "src/roma/sandbox/native_function_binding/native_function_handler.h"
#include "src/roma/sandbox/native_function_binding/native_function_table.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

using google::scp::core::os::linux::SystemResourceInfoProviderLinux;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::sandbox::constants::kRequestUuid;
using google::scp::roma::sandbox::dispatcher::Dispatcher;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionHandler;
using google::scp::roma::sandbox::native_function_binding::NativeFunctionTable;

namespace google::scp::roma::sandbox::roma_service {
constexpr int kWorkerQueueMax = 100;

// This value does not account for runtime memory usage and is only a generic
// estimate based on the memory needed by roma and the steady-state memory
// needed by v8.
constexpr uint64_t kDefaultMinStartupMemoryNeededPerWorkerKB = 400 * 1024;
constexpr uint64_t kMinWorkerVirtualMemoryMB = 10 * 1024;  // 10 GB

/**
 * @brief The template parameter, TMetadata, needs to be default assignable and
 * movable.
 */
template <typename T = google::scp::roma::DefaultMetadata>
class RomaService {
 public:
  using TMetadata = T;
  using Config = Config<TMetadata>;

  explicit RomaService(Config config = Config()) : config_(std::move(config)) {}

  // RomaService is neither copyable nor movable.
  RomaService(const RomaService&) = delete;
  RomaService& operator=(const RomaService&) = delete;

  absl::Status Init() {
    if (!RomaHasEnoughMemoryForStartup()) {
      return absl::InternalError(
          "Roma startup failed due to insufficient system memory.");
    }
    if (!RomaWorkersHaveEnoughAddressSpace()) {
      return absl::InternalError(absl::StrCat(
          "Roma startup failed due to insufficient address space for workers. "
          "Please increase config.max_worker_virtual_memory_mb above ",
          kMinWorkerVirtualMemoryMB, " MB. Current value is ",
          config_.max_worker_virtual_memory_mb, " MB."));
    }
    PS_RETURN_IF_ERROR(InitInternal());
    return absl::OkStatus();
  }

  absl::Status LoadCodeObj(std::unique_ptr<CodeObject> code_object,
                           Callback callback) {
    if (code_object->version_string.empty()) {
      return absl::InvalidArgumentError(
          "Roma LoadCodeObj failed due to invalid version.");
    }
    return dispatcher_->Load(*std::move(code_object), std::move(callback));
  }

  // Async API.
  // Execute single invocation request. Can only be called when a valid
  // code object has been loaded.
  template <typename InputType>
  absl::StatusOr<ExecutionToken> Execute(
      std::unique_ptr<InvocationRequest<InputType, TMetadata>> invocation_req,
      Callback callback) {
    // We accept empty request IDs, but we will replace them with a placeholder.
    if (invocation_req->id.empty()) {
      invocation_req->id = kDefaultRomaRequestId;
    }
    return ExecuteInternal(std::move(invocation_req), std::move(callback));
  }

  void Cancel(const ExecutionToken& token) {
    native_function_binding_handler_->PreventCallbacks(token);
    dispatcher_->Cancel(token);
  }

  // Async & Batch API.
  // Batch execute a batch of invocation requests. Can only be called when a
  // valid code object has been loaded.
  template <typename InputType>
  absl::Status BatchExecute(
      std::vector<InvocationRequest<InputType, TMetadata>>& batch,
      BatchCallback batch_callback) {
    return BatchExecuteInternal(batch, std::move(batch_callback));
  }

  absl::Status Stop() { return StopInternal(); }

 private:
  absl::Status InitInternal() {
    size_t concurrency = config_.number_of_workers;
    if (concurrency == 0) {
      concurrency = std::thread::hardware_concurrency();
    }
    size_t worker_queue_cap = config_.worker_queue_max_items;
    if (worker_queue_cap == 0) {
      worker_queue_cap = kWorkerQueueMax;
    }

    RegisterLogBindings();

    if (config_.enable_native_function_grpc_server) {
      SetupNativeFunctionGrpcServer();
    }
    PS_ASSIGN_OR_RETURN(auto native_function_binding_info,
                        SetupNativeFunctionHandler(concurrency));
    PS_RETURN_IF_ERROR(SetupWorkers(native_function_binding_info));
    native_function_binding_handler_->Run();

    // TODO: Make max_pending_requests configurable
    dispatcher_.emplace(
        absl::MakeSpan(workers_),
        concurrency * worker_queue_cap /*max_pending_requests*/);
    ROMA_VLOG(1) << "RomaService Init with " << config_.number_of_workers
                 << " workers.";
    return absl::OkStatus();
  }

  absl::Status StopInternal() noexcept {
    // Destroy dispatcher before stopping the workers to which the dispatcher
    // holds pointers.
    dispatcher_.reset();
    if (native_function_binding_handler_) {
      native_function_binding_handler_->Stop();
    }
    if (native_function_server_) {
      native_function_server_->Shutdown();
    }
    native_function_binding_table_.Clear();
    for (worker_api::WorkerSandboxApi& worker : workers_) {
      PS_RETURN_IF_ERROR(worker.Stop());
    }
    return absl::OkStatus();
  }

  void SetupNativeFunctionGrpcServer() {
    native_function_server_addresses_ = {
        absl::StrCat("unix:", std::tmpnam(nullptr), ".sock")};
    native_function_server_.emplace(&metadata_storage_,
                                    native_function_server_addresses_);

    config_.RegisterService(
        std::make_unique<grpc_server::AsyncLoggingService>(),
        grpc_server::LogHandler<TMetadata>());
    native_function_server_->AddServices(config_.ReleaseServices());
    native_function_server_->AddFactories(config_.ReleaseFactories());
    native_function_server_->Run();
  }

  absl::Status StoreMetadata(std::string uuid, TMetadata metadata) {
    return metadata_storage_.Add(std::move(uuid), std::move(metadata));
  }

  void DeleteMetadata(std::string_view uuid) {
    if (auto deletion_status = metadata_storage_.Delete(uuid);
        !deletion_status.ok()) {
      ROMA_VLOG(1) << "Failed to delete metadata. UUID: " << uuid
                   << ", status: " << deletion_status;
    }
  }

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
  absl::StatusOr<NativeFunctionBindingSetup> SetupNativeFunctionHandler(
      size_t concurrency) {
    const auto function_bindings = config_.GetFunctionBindings();

    std::vector<std::string> function_names;
    function_names.reserve(function_bindings.size());
    for (const auto& binding : function_bindings) {
      PS_RETURN_IF_ERROR(native_function_binding_table_.Register(
          binding->function_name, binding->function));
      function_names.push_back(binding->function_name);
    }

    std::vector<int> local_fds;
    local_fds.reserve(concurrency);
    std::vector<int> remote_fds;
    remote_fds.reserve(concurrency);
    for (int i = 0; i < concurrency; i++) {
      int fd_pair[2];
      if (::socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair) != 0) {
        return absl::InternalError(
            absl::StrCat("Failed to create socket for native function binding "
                         "communication."));
      }
      local_fds.push_back(fd_pair[0]);
      remote_fds.push_back(fd_pair[1]);
    }

    MetadataStorage<TMetadata>* metadata_ptr = nullptr;
    if (config_.enable_metadata_storage) {
      metadata_ptr = &metadata_storage_;
    }
    native_function_binding_handler_.emplace(
        &native_function_binding_table_, metadata_ptr, local_fds, remote_fds,
        config_.skip_callback_for_cancelled);

    NativeFunctionBindingSetup setup{
        .remote_file_descriptors = std::move(remote_fds),
        .local_file_descriptors = std::move(local_fds),
        .js_function_names = std::move(function_names),
    };
    return setup;
  }

  void RegisterLogBindings() {
    const auto log_fn_factory = [&](std::string_view function_name) {
      auto function_binding_object =
          std::make_unique<FunctionBindingObjectV2<TMetadata>>();
      function_binding_object->function_name = function_name;
      const auto severity = GetSeverity(function_name);
      function_binding_object->function =
          [severity, this](FunctionBindingPayload<TMetadata>& wrapper) {
            const auto& logging_func = config_.GetLoggingFunction();
            logging_func(severity, wrapper.metadata,
                         wrapper.io_proto.input_string());
            wrapper.io_proto.set_output_string("");
          };
      return function_binding_object;
    };
    for (const auto& name : {"ROMA_LOG", "ROMA_WARN", "ROMA_ERROR"}) {
      config_.RegisterFunctionBinding(log_fn_factory(name));
    }
  }

  absl::Status SetupWorkers(
      const NativeFunctionBindingSetup& native_binding_setup) {
    const auto& remote_fds = native_binding_setup.remote_file_descriptors;
    const auto& function_names = native_binding_setup.js_function_names;
    const auto& rpc_method_names = config_.GetRpcMethodNames();
    const auto& v8_flags = config_.GetV8Flags();
    std::string server_address = native_function_server_addresses_.empty()
                                     ? ""
                                     : native_function_server_addresses_[0];

    JsEngineResourceConstraints resource_constraints;
    config_.GetJsEngineResourceConstraints(resource_constraints);

    workers_.reserve(remote_fds.size());
    for (const int remote_fd : remote_fds) {
      workers_.emplace_back(
          /*require_preload=*/true,
          /*native_js_function_comms_fd=*/remote_fd,
          /*native_js_function_names=*/function_names,
          /*rpc_method_names=*/rpc_method_names,
          /*server_address=*/server_address,
          /*max_worker_virtual_memory_mb=*/config_.max_worker_virtual_memory_mb,
          /*js_engine_initial_heap_size_mb=*/
          resource_constraints.initial_heap_size_in_mb,
          /*js_engine_maximum_heap_size_mb=*/
          resource_constraints.maximum_heap_size_in_mb,
          /*js_engine_max_wasm_memory_number_of_pages=*/
          config_.max_wasm_memory_number_of_pages,
          /*sandbox_request_response_shared_buffer_size_mb=*/
          config_.sandbox_request_response_shared_buffer_size_mb,
          /*enable_sandbox_sharing_request_response_with_buffer_only=*/
          config_.enable_sandbox_sharing_request_response_with_buffer_only,
          /*v8_flags=*/v8_flags,
          /*enable_profilers=*/config_.enable_profilers,
          /*logging_function_set=*/config_.logging_function_set,
          /*disable_udf_stacktraces_in_response*/
          config_.disable_udf_stacktraces_in_response);
      PS_RETURN_IF_ERROR(workers_.back().Init());
      PS_RETURN_IF_ERROR(workers_.back().Run());
    }
    return absl::OkStatus();
  }

  template <typename InputType>
  absl::Status AssertInvocationRequestIsValid(
      std::string_view function_name,
      const InvocationRequest<InputType, TMetadata>& invocation_req) {
    if (invocation_req.version_string.empty()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Roma ", function_name, " failed due to invalid version."));
    }

    if (invocation_req.handler_name.empty()) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Roma ", function_name, " failed due to empty handler name."));
    }

    return absl::OkStatus();
  }

  template <typename InputType>
  absl::StatusOr<ExecutionToken> ExecuteInternal(
      std::unique_ptr<InvocationRequest<InputType, TMetadata>> invocation_req,
      Callback callback) {
    PS_RETURN_IF_ERROR(
        AssertInvocationRequestIsValid("Execute", *invocation_req));

    auto request_unique_id = google::scp::core::common::Uuid::GenerateUuid();
    std::string uuid_str =
        google::scp::core::common::ToString(request_unique_id);
    invocation_req->tags.insert({std::string(kRequestUuid), uuid_str});

    invocation_req->tags.insert(
        {std::string(google::scp::roma::sandbox::constants::kMinLogLevel),
         absl::StrCat(invocation_req->min_log_level)});

    Callback callback_wrapper =
        [this, uuid_str, callback = std::move(callback)](
            absl::StatusOr<ResponseObject> resp) mutable {
          callback(std::move(resp));
          DeleteMetadata(uuid_str);
        };

    PS_RETURN_IF_ERROR(
        StoreMetadata(uuid_str, std::move(invocation_req->metadata)));
    PS_RETURN_IF_ERROR(dispatcher_->Invoke(std::move(*invocation_req),
                                           std::move(callback_wrapper)));
    return ExecutionToken{std::move(uuid_str)};
  }

  template <typename InputType>
  absl::Status BatchExecuteInternal(
      std::vector<InvocationRequest<InputType, TMetadata>>& batch,
      BatchCallback batch_callback) {
    std::vector<std::string> uuids;
    uuids.reserve(batch.size());

    for (auto& request : batch) {
      PS_RETURN_IF_ERROR(
          AssertInvocationRequestIsValid("BatchExecute", request));
      auto request_unique_id = google::scp::core::common::Uuid::GenerateUuid();
      std::string uuid_str =
          google::scp::core::common::ToString(request_unique_id);
      // Save uuids for later removal in callback_wrapper
      uuids.push_back(uuid_str);
      auto [it, inserted] =
          request.tags.insert({std::string(kRequestUuid), uuid_str});
      if (!inserted) {
        it->second = uuid_str;
      }
      PS_RETURN_IF_ERROR(
          StoreMetadata(std::move(uuid_str), std::move(request.metadata)));
    }
    auto batch_callback_ptr = std::make_shared<BatchCallback>(
        [&, uuids = std::move(uuids),
         batch_callback = std::move(batch_callback)](
            std::vector<absl::StatusOr<ResponseObject>> batch_resp) mutable {
          std::move(batch_callback)(std::move(batch_resp));
          for (const auto& uuid : uuids) {
            DeleteMetadata(uuid);
          }
        });
    const auto batch_size = batch.size();
    auto batch_response =
        std::make_shared<std::vector<absl::StatusOr<ResponseObject>>>(
            batch_size, absl::StatusOr<ResponseObject>());
    auto finished_counter = std::make_shared<std::atomic<size_t>>(0);
    for (size_t index = 0; index < batch_size; ++index) {
      auto callback = [batch_response, finished_counter, batch_callback_ptr,
                       index](absl::StatusOr<ResponseObject> obj_response) {
        (*batch_response)[index] = std::move(obj_response);
        auto finished_value = finished_counter->fetch_add(1);
        if (finished_value + 1 == batch_response->size()) {
          (*batch_callback_ptr)(std::move(*batch_response));
        }
      };
      absl::Status result;
      while (!(result = dispatcher_->Invoke(batch[index], callback)).ok()) {
        // If the first request from the batch got a failure, return failure
        // without waiting.
        if (index == 0) {
          return result;
        }
      }
    }
    return absl::OkStatus();
  }

  // V8 fails to initialize if Roma workers aren't given at least
  // kMinWorkerVirtualMemoryMB of address space.
  bool RomaWorkersHaveEnoughAddressSpace() {
    return config_.max_worker_virtual_memory_mb == 0 ||
           config_.max_worker_virtual_memory_mb >= kMinWorkerVirtualMemoryMB;
  }

  bool RomaHasEnoughMemoryForStartup() {
    if (!config_.enable_startup_memory_check) {
      return true;
    }

    SystemResourceInfoProviderLinux mem_info;
    auto available_memory = mem_info.GetAvailableMemoryKb();
    ROMA_VLOG(1) << "Available memory is " << *available_memory << " Kb";
    if (!available_memory.result().Successful()) {
      // Failing to read the meminfo file should not stop startup.
      // This mem check is a best-effort check.
      return true;
    }

    if (config_.GetStartupMemoryCheckMinimumNeededValueKb) {
      return config_.GetStartupMemoryCheckMinimumNeededValueKb() <
             *available_memory;
    }

    auto cpu_count = std::thread::hardware_concurrency();
    auto num_processes = (config_.number_of_workers > 0 &&
                          config_.number_of_workers <= cpu_count)
                             ? config_.number_of_workers
                             : cpu_count;

    ROMA_VLOG(1) << "Number of workers is " << num_processes;

    auto minimum_memory_needed =
        num_processes * kDefaultMinStartupMemoryNeededPerWorkerKB;

    return minimum_memory_needed < *available_memory;
  }

  absl::LogSeverity GetSeverity(std::string_view severity) {
    if (severity == "ROMA_LOG") {
      return absl::LogSeverity::kInfo;
    } else if (severity == "ROMA_WARN") {
      return absl::LogSeverity::kWarning;
    } else {
      return absl::LogSeverity::kError;
    }
  }

  Config config_;
  std::vector<worker_api::WorkerSandboxApi> workers_;
  native_function_binding::NativeFunctionTable<TMetadata>
      native_function_binding_table_;
  // Map of invocation request uuid to associated metadata.
  MetadataStorage<TMetadata> metadata_storage_;
  std::optional<NativeFunctionHandler<TMetadata>>
      native_function_binding_handler_;
  std::optional<dispatcher::Dispatcher> dispatcher_;
  std::vector<std::string> native_function_server_addresses_;
  std::optional<grpc_server::NativeFunctionGrpcServer<TMetadata>>
      native_function_server_;
};
}  // namespace google::scp::roma::sandbox::roma_service

#endif  // ROMA_SANDBOX_ROMA_SERVICE_ROMA_SERVICE_H_
