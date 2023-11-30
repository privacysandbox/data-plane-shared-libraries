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

#include "worker.h"

#include <unistd.h>

#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker/src/worker_utils.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_WORKER_MISSING_CONTEXT_WHEN_EXECUTING;
using google::scp::core::errors::SC_ROMA_WORKER_MISSING_METADATA_ITEM;
using google::scp::core::errors::SC_ROMA_WORKER_REQUEST_TYPE_NOT_SUPPORTED;
using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestActionLoad;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using google::scp::roma::sandbox::constants::kRequestTypeJavascriptWithWasm;
using google::scp::roma::sandbox::constants::kRequestTypeWasm;
using google::scp::roma::sandbox::js_engine::JsEngineExecutionResponse;
using google::scp::roma::sandbox::js_engine::RomaJsEngineCompilationContext;

namespace google::scp::roma::sandbox::worker {
Worker::Worker(std::unique_ptr<js_engine::JsEngine> js_engine,
               bool require_preload, size_t compilation_context_cache_size)
    : js_engine_(std::move(js_engine)),
      require_preload_(require_preload),
      compilation_contexts_(compilation_context_cache_size) {
  CHECK(compilation_context_cache_size > 0)
      << "compilation_context_cache_size cannot be zero";
}

ExecutionResult Worker::Run() noexcept {
  return js_engine_->Run();
}

ExecutionResult Worker::Stop() noexcept {
  return js_engine_->Stop();
}

ExecutionResultOr<js_engine::ExecutionResponse> Worker::RunCode(
    const std::string& code, const std::vector<absl::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    absl::Span<const uint8_t> wasm) {
  auto request_type_or =
      WorkerUtils::GetValueFromMetadata(metadata, kRequestType);
  RETURN_IF_FAILURE(request_type_or.result());

  ROMA_VLOG(2) << "Worker executing request with code type of "
               << *request_type_or;

  auto code_version_or =
      WorkerUtils::GetValueFromMetadata(metadata, kCodeVersion);
  RETURN_IF_FAILURE(code_version_or.result());

  ROMA_VLOG(2) << "Worker executing request with code version of "
               << *code_version_or;

  auto action_or = WorkerUtils::GetValueFromMetadata(metadata, kRequestAction);
  RETURN_IF_FAILURE(action_or.result());

  ROMA_VLOG(2) << "Worker executing request with action of " << *action_or;

  std::string handler_name = "";
  auto handler_name_or =
      WorkerUtils::GetValueFromMetadata(metadata, kHandlerName);

  // If we read the handler name successfully, let's store it.
  // Else if we didn't read it and the request is not a load request,
  // then return the failure.
  if (handler_name_or.result().Successful()) {
    handler_name = *handler_name_or;
  } else if (*action_or != kRequestActionLoad) {
    return handler_name_or.result();
  }

  ROMA_VLOG(2) << "Worker executing request with handler name " << handler_name;

  RomaJsEngineCompilationContext context;

  // Only reuse the context if this is not a load request.
  // A load request for an existing version should overwrite the given version.
  const std::string code_version(*code_version_or);
  if (*action_or != kRequestActionLoad) {
    // To support `require_preload_` we must perform a double lookup.
    absl::MutexLock l(&cache_mu_);
    if (require_preload_ && !compilation_contexts_.Contains(code_version)) {
      // If we require preloads and we couldn't find a context and this is not a
      // load request, then bail out. This is an execution without a previous
      // load.
      return FailureExecutionResult(
          SC_ROMA_WORKER_MISSING_CONTEXT_WHEN_EXECUTING);
    } else {
      context = compilation_contexts_.Get(code_version);
    }
  }

  core::ExecutionResultOr<JsEngineExecutionResponse> response_or;
  if (*request_type_or == kRequestTypeJavascript) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript object";

    response_or = js_engine_->CompileAndRunJs(code, handler_name, input,
                                              metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else if (*request_type_or == kRequestTypeWasm) {
    ROMA_VLOG(2) << "Worker executing request as WASM object";

    response_or = js_engine_->CompileAndRunWasm(code, handler_name, input,
                                                metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else if (*request_type_or == kRequestTypeJavascriptWithWasm) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript with WASM object";

    response_or = js_engine_->CompileAndRunJsWithWasm(code, wasm, handler_name,
                                                      input, metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else {
    return FailureExecutionResult(SC_ROMA_WORKER_REQUEST_TYPE_NOT_SUPPORTED);
  }

  if (*action_or == kRequestActionLoad && response_or->compilation_context) {
    absl::MutexLock l(&cache_mu_);
    compilation_contexts_.Set(code_version, response_or->compilation_context);
    ROMA_VLOG(1) << "caching compilation context for version " << code_version;
  }
  return response_or->execution_response;
}
}  // namespace google::scp::roma::sandbox::worker
