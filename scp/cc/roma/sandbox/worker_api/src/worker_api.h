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

#ifndef ROMA_SANDBOX_WORKER_API_SRC_WORKER_API_H_
#define ROMA_SANDBOX_WORKER_API_SRC_WORKER_API_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "core/interface/service_interface.h"
#include "public/core/interface/execution_result.h"

namespace google::scp::roma::sandbox::worker_api {
class WorkerApi {
 public:
  struct RunCodeRequest {
    absl::string_view code;
    std::vector<absl::string_view> input;
    absl::flat_hash_map<std::string, std::string> metadata;
    absl::Span<const uint8_t> wasm;
  };

  struct RunCodeResponse {
    std::shared_ptr<std::string> response;
    std::vector<std::shared_ptr<std::string>> errors;
    absl::flat_hash_map<std::string, absl::Duration> metrics;
  };

  virtual absl::Status Init() noexcept = 0;
  virtual absl::Status Run() noexcept = 0;
  virtual absl::Status Stop() noexcept = 0;

  // Whether a request should be retried or not.
  enum class RetryStatus { kDoNotRetry, kRetry };

  /**
   * @brief Method to execute a code request.
   * @note The implementation of this method must be thread safe.
   * @returns a pair of result and whether this request should be retried
   */
  virtual std::pair<core::ExecutionResultOr<RunCodeResponse>, RetryStatus>
  RunCode(const RunCodeRequest& request) noexcept = 0;

  /**
   * @brief Terminate the underlying worker.
   */
  virtual void Terminate() noexcept = 0;
};
}  // namespace google::scp::roma::sandbox::worker_api

#endif  // ROMA_SANDBOX_WORKER_API_SRC_WORKER_API_H_
