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
#include "absl/strings/str_cat.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/util/status_macro/status_macros.h"

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

void Worker::Run() { js_engine_->Run(); }

void Worker::Stop() { js_engine_->Stop(); }

absl::StatusOr<js_engine::ExecutionResponse> Worker::RunCode(
    std::string_view code, const std::vector<std::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    absl::Span<const uint8_t> wasm) {
  auto code_version_it = metadata.find(kCodeVersion);
  if (code_version_it == metadata.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Missing expected key in request metadata: ", kCodeVersion));
  }
  std::string_view code_version = code_version_it->second;

  ROMA_VLOG(2) << "Worker executing request with code version of "
               << code_version;

  auto action_it = metadata.find(kRequestAction);
  if (action_it == metadata.end()) {
    return absl::InvalidArgumentError(absl::StrCat(
        "Missing expected key in request metadata: ", kRequestAction));
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
    return absl::InvalidArgumentError(absl::StrCat(
        "Missing expected key in request metadata: ", kRequestActionLoad));
  }

  ROMA_VLOG(2) << "Worker executing request with handler name " << handler_name;

  std::string_view request_type;
  if (const auto request_type_it = metadata.find(kRequestType);
      request_type_it != metadata.end()) {
    request_type = request_type_it->second;
  }
  RomaJsEngineCompilationContext context;

  // Only reuse the compilation context if this is not a load request.
  // A load request for an existing version should overwrite the given version.
  if (action != kRequestActionLoad) {
    absl::MutexLock lock(&cache_mu_);
    if (const auto it = compilation_contexts_.find(code_version);
        it != compilation_contexts_.end()) {
      context = it->second.context;
      request_type = it->second.request_type;
    } else if (require_preload_) {
      // If we require preloads and we couldn't find a context and this is not a
      // load request, then bail out. This is an execution without a previous
      // load.
      return absl::NotFoundError(
          "Could not find a stored context for the execution request.");
    }
  }

  absl::StatusOr<JsEngineExecutionResponse> response_or;
  if (request_type == kRequestTypeJavascript) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript object";

    response_or = js_engine_->CompileAndRunJs(code, handler_name, input,
                                              metadata, context);
    PS_RETURN_IF_ERROR(response_or.status());
  } else if (request_type == kRequestTypeWasm) {
    ROMA_VLOG(2) << "Worker executing request as WASM object";

    response_or = js_engine_->CompileAndRunWasm(code, handler_name, input,
                                                metadata, context);
    PS_RETURN_IF_ERROR(response_or.status());
  } else if (request_type == kRequestTypeJavascriptWithWasm) {
    ROMA_VLOG(2) << "Worker executing request as JavaScript with WASM object";

    response_or = js_engine_->CompileAndRunJsWithWasm(code, wasm, handler_name,
                                                      input, metadata, context);
    PS_RETURN_IF_ERROR(response_or.status());
  } else {
    return absl::UnimplementedError("The request type is not yet supported.");
  }

  if (action == kRequestActionLoad && response_or->compilation_context) {
    absl::MutexLock lock(&cache_mu_);
    compilation_contexts_[code_version] = CacheEntry{
        .context = std::move(response_or)->compilation_context,
        .request_type = std::string(request_type),
    };
    ROMA_VLOG(1) << "caching compilation context for version " << code_version;
  }
  return response_or->execution_response;
}

}  // namespace google::scp::roma::sandbox::worker
