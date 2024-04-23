// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef CORE_ASYNC_EXECUTOR_ASYNC_EXECUTOR_UTILS_H_
#define CORE_ASYNC_EXECUTOR_ASYNC_EXECUTOR_UTILS_H_

#include <iostream>
#include <optional>
#include <thread>

#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"

#include "error_codes.h"

namespace google::scp::core {

class AsyncExecutorUtils {
 public:
  /// Sets the affinity of the current thread to that cpu number.
  static inline ExecutionResult SetAffinity(size_t cpu_number) noexcept {
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(cpu_number, &cpuset);
    int rc = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
      auto result = FailureExecutionResult(
          errors::SC_ASYNC_EXECUTOR_UNABLE_TO_SET_AFFINITY);
      SCP_ERROR(kAsyncExecutorUtils, common::kZeroUuid, result,
                "SetAffinity pthread_setaffinity_np failed: %s", strerror(rc));
      return result;
    }
    return SuccessExecutionResult();
  }

 private:
  static constexpr std::string_view kAsyncExecutorUtils = "AsyncExecutorUtils";
};

}  // namespace google::scp::core

#endif  // CORE_ASYNC_EXECUTOR_ASYNC_EXECUTOR_UTILS_H_
