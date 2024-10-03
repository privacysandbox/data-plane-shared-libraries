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

#include "utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/roma/config/config.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_function_binding.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_js_engine.h"
#include "src/roma/sandbox/native_function_binding/native_function_invoker.h"
#include "src/roma/sandbox/worker/worker.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupV8FlagsKey;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateFunctionBinding;
using google::scp::roma::sandbox::js_engine::v8_js_engine::V8JsEngine;
using google::scp::roma::sandbox::native_function_binding::
    NativeFunctionInvoker;
using google::scp::roma::sandbox::worker::Worker;

namespace google::scp::roma::sandbox::worker_api {

absl::flat_hash_map<std::string, std::string> GetEngineOneTimeSetup(
    const V8WorkerEngineParams& params) {
  return {
      {std::string(kJsEngineOneTimeSetupWasmPagesKey),
       absl::StrCat(params.max_wasm_memory_number_of_pages)},
      {std::string(kJsEngineOneTimeSetupV8FlagsKey),
       absl::StrJoin(params.v8_flags, " ")},
  };
}

std::unique_ptr<Worker> CreateWorker(const V8WorkerEngineParams& params) {
  auto native_function_invoker = std::make_unique<NativeFunctionInvoker>(
      params.native_js_function_comms_fd);

  auto isolate_function_binding = std::make_unique<V8IsolateFunctionBinding>(
      params.native_js_function_names, params.rpc_method_names,
      std::move(native_function_invoker), params.server_address);

  auto v8_engine = std::make_unique<V8JsEngine>(
      std::move(isolate_function_binding), params.skip_v8_cleanup,
      params.enable_profilers, params.resource_constraints,
      params.logging_function_set, params.disable_udf_stacktraces_in_response);
  v8_engine->OneTimeSetup(GetEngineOneTimeSetup(params));
  return std::make_unique<Worker>(std::move(v8_engine), params.require_preload);
}

void ClearInputFields(::worker_api::WorkerParamsProto& params) {
  params.clear_code();
  params.clear_input_strings();
  params.clear_input_bytes();
}

std::pair<absl::Status, RetryStatus> WrapResultWithNoRetry(
    absl::Status result) {
  return std::make_pair(std::move(result), RetryStatus::kDoNotRetry);
}

std::pair<absl::Status, RetryStatus> WrapResultWithRetry(absl::Status result) {
  return std::make_pair(std::move(result), RetryStatus::kRetry);
}

}  // namespace google::scp::roma::sandbox::worker_api
