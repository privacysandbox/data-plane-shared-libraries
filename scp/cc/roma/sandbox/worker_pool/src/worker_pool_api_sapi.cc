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

#include "worker_pool_api_sapi.h"

#include <vector>

#include "absl/log/check.h"

namespace google::scp::roma::sandbox::worker_pool {

using core::errors::GetErrorMessage;

WorkerPoolApiSapi::WorkerPoolApiSapi(
    const std::vector<worker_api::WorkerApiSapiConfig>& configs) {
  for (auto config : configs) {
    workers_.push_back(std::make_unique<worker_api::WorkerApiSapi>(config));
  }
}

absl::Status WorkerPoolApiSapi::Init() {
  for (auto& w : workers_) {
    auto result = w->Init();
    if (!result.ok()) {
      return result;
    }
  }

  return absl::OkStatus();
}

absl::Status WorkerPoolApiSapi::Run() {
  for (auto& w : workers_) {
    auto result = w->Run();
    if (!result.ok()) {
      return result;
    }
  }

  return absl::OkStatus();
}

absl::Status WorkerPoolApiSapi::Stop() {
  for (auto& w : workers_) {
    auto result = w->Stop();
    if (!result.ok()) {
      return result;
    }
  }

  return absl::OkStatus();
}

size_t WorkerPoolApiSapi::GetPoolSize() {
  return workers_.size();
}

absl::StatusOr<worker_api::WorkerApi*> WorkerPoolApiSapi::GetWorker(
    size_t index) {
  if (index >= workers_.size()) {
    return absl::OutOfRangeError(
        absl::StrCat("The worker index was out of bounds: ", index));
  }

  return workers_.at(index).get();
}
}  // namespace google::scp::roma::sandbox::worker_pool
