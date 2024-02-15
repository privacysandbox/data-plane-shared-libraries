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

#ifndef ROMA_SANDBOX_WORKER_API_SRC_WORKER_API_SAPI_H_
#define ROMA_SANDBOX_WORKER_API_SRC_WORKER_API_SAPI_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "roma/config/src/config.h"
#include "roma/sandbox/worker_api/sapi/src/worker_sandbox_api.h"

#include "worker_api.h"

namespace google::scp::roma::sandbox::worker_api {

struct WorkerApiSapiConfig {
  bool js_engine_require_code_preload;
  int native_js_function_comms_fd;
  std::vector<std::string> native_js_function_names;
  size_t max_worker_virtual_memory_mb;
  JsEngineResourceConstraints js_engine_resource_constraints;
  size_t js_engine_max_wasm_memory_number_of_pages;
  size_t sandbox_request_response_shared_buffer_size_mb;
  bool enable_sandbox_sharing_request_response_with_buffer_only;
};

class WorkerApiSapi : public WorkerApi {
 public:
  explicit WorkerApiSapi(const WorkerApiSapiConfig& config);

  absl::Status Init() noexcept override ABSL_LOCKS_EXCLUDED(run_code_mutex_);

  absl::Status Run() noexcept override ABSL_LOCKS_EXCLUDED(run_code_mutex_);

  absl::Status Stop() noexcept override ABSL_LOCKS_EXCLUDED(run_code_mutex_);

  std::pair<core::ExecutionResultOr<RunCodeResponse>, RetryStatus> RunCode(
      const WorkerApi::RunCodeRequest& request) noexcept override
      ABSL_LOCKS_EXCLUDED(run_code_mutex_);

  void Terminate() noexcept override ABSL_LOCKS_EXCLUDED(run_code_mutex_);

 private:
  WorkerSandboxApi sandbox_api_ ABSL_GUARDED_BY(run_code_mutex_);
  absl::Mutex run_code_mutex_;
};
}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SRC_WORKER_API_SAPI_H_
