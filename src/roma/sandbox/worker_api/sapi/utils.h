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

#ifndef ROMA_SANDBOX_WORKER_API_SAPI_UTILS_H_
#define ROMA_SANDBOX_WORKER_API_SAPI_UTILS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/roma/config/config.h"
#include "src/roma/sandbox/worker/worker.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

namespace google::scp::roma::sandbox::worker_api {

enum class RetryStatus { kDoNotRetry, kRetry };

struct V8WorkerEngineParams {
  int native_js_function_comms_fd;
  std::vector<std::string> native_js_function_names;
  std::vector<std::string> rpc_method_names;
  std::string server_address;
  google::scp::roma::JsEngineResourceConstraints resource_constraints;
  size_t max_wasm_memory_number_of_pages;
  bool require_preload = true;
  size_t compilation_context_cache_size;
  bool skip_v8_cleanup = false;
  std::vector<std::string> v8_flags;
  bool enable_profilers = false;
  bool logging_function_set = false;
  bool disable_udf_stacktraces_in_response = false;
};

absl::flat_hash_map<std::string, std::string> GetEngineOneTimeSetup(
    const V8WorkerEngineParams& params);

std::unique_ptr<google::scp::roma::sandbox::worker::Worker> CreateWorker(
    const V8WorkerEngineParams& params);

void ClearInputFields(::worker_api::WorkerParamsProto& params);

std::pair<absl::Status, RetryStatus> WrapResultWithNoRetry(absl::Status result);

std::pair<absl::Status, RetryStatus> WrapResultWithRetry(absl::Status result);

}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SAPI_UTILS_H_
