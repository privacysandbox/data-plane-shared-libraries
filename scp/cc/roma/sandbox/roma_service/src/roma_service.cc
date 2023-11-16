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

#include "roma_service.h"

#include <functional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "roma/logging/src/logging.h"
#include "roma/sandbox/worker_api/src/worker_api_sapi.h"
#include "roma/sandbox/worker_pool/src/worker_pool_api_sapi.h"

#include "error_codes.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_SERVICE_COULD_NOT_CREATE_FD_PAIR;
using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::sandbox::dispatcher::Dispatcher;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionHandlerSapiIpc;
using google::scp::roma::sandbox::native_function_binding::NativeFunctionTable;
using google::scp::roma::sandbox::worker_api::WorkerApiSapi;
using google::scp::roma::sandbox::worker_api::WorkerApiSapiConfig;
using google::scp::roma::sandbox::worker_pool::WorkerPoolApiSapi;

namespace google::scp::roma::sandbox::roma_service {

absl::Mutex RomaService::instance_mu_;
RomaService* RomaService::instance_;
constexpr int kWorkerQueueMax = 100;

namespace {

void RomaLog(const FunctionBindingIoProto& io) {
  LOG(INFO) << io.input_string();
}

void RomaWarn(const FunctionBindingIoProto& io) {
  LOG(WARNING) << io.input_string();
}

void RomaError(const FunctionBindingIoProto& io) {
  LOG(ERROR) << io.input_string();
}

}  // namespace

void RomaService::RegisterLogBindings() noexcept {
  absl::flat_hash_map<std::string, std::function<void(FunctionBindingIoProto)>>
      cpp_to_js_names = {
          {"ROMA_LOG", RomaLog},
          {"ROMA_WARN", RomaWarn},
          {"ROMA_ERROR", RomaError},
      };

  const auto log_fn_factory = [&cpp_to_js_names](
                                  const std::string& function_name) {
    auto function_binding_object = std::make_unique<FunctionBindingObjectV2>();
    function_binding_object->function = cpp_to_js_names[function_name];
    function_binding_object->function_name = function_name;
    return function_binding_object;
  };
  for (const auto& name : {"ROMA_LOG", "ROMA_WARN", "ROMA_ERROR"}) {
    config_.RegisterFunctionBinding(log_fn_factory(name));
  }
}

ExecutionResult RomaService::Init() noexcept {
  size_t concurrency = config_.number_of_workers;
  if (concurrency == 0) {
    concurrency = std::thread::hardware_concurrency();
  }

  size_t worker_queue_cap = config_.worker_queue_max_items;
  if (worker_queue_cap == 0) {
    worker_queue_cap = kWorkerQueueMax;
  }

  RegisterLogBindings();
  auto native_function_binding_info_or =
      SetupNativeFunctionHandler(concurrency);
  RETURN_IF_FAILURE(native_function_binding_info_or.result());

  RETURN_IF_FAILURE(SetupWorkers(*native_function_binding_info_or));

  async_executor_ =
      std::make_unique<AsyncExecutor>(concurrency, worker_queue_cap);
  RETURN_IF_FAILURE(async_executor_->Init());

  // TODO: Make max_pending_requests configurable
  dispatcher_ = std::make_unique<class Dispatcher>(
      async_executor_.get(), worker_pool_.get(),
      concurrency * worker_queue_cap /*max_pending_requests*/,
      config_.code_version_cache_size);
  RETURN_IF_FAILURE(dispatcher_->Init());
  ROMA_VLOG(1) << "RomaService Init with " << config_.number_of_workers
               << " workers. The capacity of code cache is "
               << config_.code_version_cache_size;
  return SuccessExecutionResult();
}

ExecutionResult RomaService::Run() noexcept {
  RETURN_IF_FAILURE(native_function_binding_handler_->Run());
  RETURN_IF_FAILURE(async_executor_->Run());
  RETURN_IF_FAILURE(worker_pool_->Run());
  RETURN_IF_FAILURE(dispatcher_->Run());
  return SuccessExecutionResult();
}

ExecutionResult RomaService::Stop() noexcept {
  if (native_function_binding_handler_) {
    RETURN_IF_FAILURE(native_function_binding_handler_->Stop());
  }
  native_function_binding_table_.Clear();
  if (dispatcher_) {
    RETURN_IF_FAILURE(dispatcher_->Stop());
  }
  if (worker_pool_) {
    RETURN_IF_FAILURE(worker_pool_->Stop());
  }
  if (async_executor_) {
    RETURN_IF_FAILURE(async_executor_->Stop());
  }
  return SuccessExecutionResult();
}

ExecutionResultOr<RomaService::NativeFunctionBindingSetup>
RomaService::SetupNativeFunctionHandler(size_t concurrency) {
  std::vector<std::shared_ptr<FunctionBindingObjectV2>> function_bindings;
  config_.GetFunctionBindings(function_bindings);

  std::vector<std::string> function_names;
  for (auto& binding : function_bindings) {
    RETURN_IF_FAILURE(native_function_binding_table_.Register(
        binding->function_name, binding->function));
    function_names.push_back(binding->function_name);
  }

  std::vector<int> local_fds;
  std::vector<int> remote_fds;
  for (int i = 0; i < concurrency; i++) {
    int fd_pair[2];
    if (socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fd_pair) != 0) {
      return FailureExecutionResult(SC_ROMA_SERVICE_COULD_NOT_CREATE_FD_PAIR);
    }
    local_fds.push_back(fd_pair[0]);
    remote_fds.push_back(fd_pair[1]);
  }

  native_function_binding_handler_ =
      std::make_unique<NativeFunctionHandlerSapiIpc>(
          &native_function_binding_table_, local_fds, remote_fds);
  RETURN_IF_FAILURE(native_function_binding_handler_->Init());

  NativeFunctionBindingSetup setup{
      .remote_file_descriptors = std::move(remote_fds),
      .local_file_descriptors = local_fds,
      .js_function_names = function_names,
  };
  return setup;
}

ExecutionResult RomaService::SetupWorkers(
    const NativeFunctionBindingSetup& native_binding_setup) {
  std::vector<WorkerApiSapiConfig> worker_configs;
  const auto& remote_fds = native_binding_setup.remote_file_descriptors;
  const auto& function_names = native_binding_setup.js_function_names;

  JsEngineResourceConstraints resource_constraints;
  config_.GetJsEngineResourceConstraints(resource_constraints);

  for (const int remote_fd : remote_fds) {
    WorkerApiSapiConfig worker_api_sapi_config{
        .worker_js_engine = worker::WorkerFactory::WorkerEngine::v8,
        .js_engine_require_code_preload = true,
        .compilation_context_cache_size = config_.code_version_cache_size,
        .native_js_function_comms_fd = remote_fd,
        .native_js_function_names = function_names,
        .max_worker_virtual_memory_mb = config_.max_worker_virtual_memory_mb,
        .js_engine_resource_constraints = resource_constraints,
        .js_engine_max_wasm_memory_number_of_pages =
            config_.max_wasm_memory_number_of_pages,
        .sandbox_request_response_shared_buffer_size_mb =
            config_.sandbox_request_response_shared_buffer_size_mb,
        .enable_sandbox_sharing_request_response_with_buffer_only =
            config_.enable_sandbox_sharing_request_response_with_buffer_only,
    };
    worker_configs.push_back(worker_api_sapi_config);
  }
  worker_pool_ = std::make_unique<WorkerPoolApiSapi>(worker_configs);
  return worker_pool_->Init();
}

}  // namespace google::scp::roma::sandbox::roma_service
