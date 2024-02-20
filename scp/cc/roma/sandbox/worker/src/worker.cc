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
#include "scp/cc/roma/logging/src/logging.h"
#include "scp/cc/roma/sandbox/constants/constants.h"

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
               bool require_preload)
    : js_engine_(std::move(js_engine)), require_preload_(require_preload) {}

void Worker::Run() {
  js_engine_->Run();
}

void Worker::Stop() {
  js_engine_->Stop();
}

ExecutionResultOr<js_engine::ExecutionResponse> Worker::RunCode(
    std::string_view code, const std::vector<std::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    absl::Span<const uint8_t> wasm) {
  auto request_type_it = metadata.find(kRequestType);
  if (request_type_it == metadata.end()) {
    return FailureExecutionResult(SC_ROMA_WORKER_MISSING_METADATA_ITEM);
  }
  std::string_view request_type = request_type_it->second;

  ROMA_VLOG(2) << "Worker executing request with code type of " << request_type;

  auto code_version_it = metadata.find(kCodeVersion);
  if (code_version_it == metadata.end()) {
    return FailureExecutionResult(SC_ROMA_WORKER_MISSING_METADATA_ITEM);
  }
  std::string_view code_version = code_version_it->second;

  ROMA_VLOG(2) << "Worker executing request with code version of "
               << code_version;

  auto action_it = metadata.find(kRequestAction);
  if (action_it == metadata.end()) {
    return FailureExecutionResult(SC_ROMA_WORKER_MISSING_METADATA_ITEM);
  }
  std::string_view action = action_it->second;

  ROMA_VLOG(2) << "Worker executing request with action of " << action;

  std::string_view handler_name = "";
  auto handler_name_it = metadata.find(kHandlerName);

  // If we read the handler name successfully, let's store it.
  // Else if we didn't read it and the request is not a load request,
  // then return the failure.
  if (handler_name_it != metadata.end()) {
    handler_name = handler_name_it->second;
  } else if (action != kRequestActionLoad) {
    return FailureExecutionResult(SC_ROMA_WORKER_MISSING_METADATA_ITEM);
  }

  ROMA_VLOG(2) << "Worker executing request with handler name " << handler_name;

  RomaJsEngineCompilationContext context;

  // Only reuse the context if this is not a load request.
  // A load request for an existing version should overwrite the given version.
  if (action != kRequestActionLoad) {
    absl::MutexLock l(&cache_mu_);
    if (const auto it = compilation_contexts_.find(code_version);
        it != compilation_contexts_.end()) {
      context = it->second;
    } else if (require_preload_) {
      // If we require preloads and we couldn't find a context and this is not a
      // load request, then bail out. This is an execution without a previous
      // load.
      return FailureExecutionResult(
          SC_ROMA_WORKER_MISSING_CONTEXT_WHEN_EXECUTING);
    }
  }

  core::ExecutionResultOr<JsEngineExecutionResponse> response_or;
  if (request_type == kRequestTypeJavascript) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript object";

    response_or = js_engine_->CompileAndRunJs(code, handler_name, input,
                                              metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else if (request_type == kRequestTypeWasm) {
    ROMA_VLOG(2) << "Worker executing request as WASM object";

    response_or = js_engine_->CompileAndRunWasm(code, handler_name, input,
                                                metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else if (request_type == kRequestTypeJavascriptWithWasm) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript with WASM object";

    response_or = js_engine_->CompileAndRunJsWithWasm(code, wasm, handler_name,
                                                      input, metadata, context);
    RETURN_IF_FAILURE(response_or.result());
  } else {
    return FailureExecutionResult(SC_ROMA_WORKER_REQUEST_TYPE_NOT_SUPPORTED);
  }

  if (action == kRequestActionLoad && response_or->compilation_context) {
    absl::MutexLock l(&cache_mu_);
    compilation_contexts_[code_version] = response_or->compilation_context;
    ROMA_VLOG(1) << "caching compilation context for version " << code_version;
  }
  return response_or->execution_response;
}
}  // namespace google::scp::roma::sandbox::worker
