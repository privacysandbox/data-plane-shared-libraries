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

#ifndef CORE_ASYNC_EXECUTOR_ASYNC_TASK_H_
#define CORE_ASYNC_EXECUTOR_ASYNC_TASK_H_

#include <memory>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core {
/**
 * @brief  Is used by the async executor to encapsulate the async operations
 * provided by the user.
 */
class AsyncTask {
 public:
  /**
   * @brief Construct a new Async Task object. By default the execution time
   * will be set to the current time.
   *
   * @param async_operation The async operation to be executed.
   */
  AsyncTask(
      AsyncOperation async_operation = []() {},
      Timestamp execution_timestamp =
          common::TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks())
      : async_operation_(std::move(async_operation)),
        execution_timestamp_(execution_timestamp),
        is_cancelled_(false) {}

  /**
   * @brief Returns the execution time of the current task.
   *
   * @return Timestamp
   */
  Timestamp GetExecutionTimestamp() const { return execution_timestamp_; }

  /// Calls the current task to be executed.
  void Execute() ABSL_LOCKS_EXCLUDED(cancellation_mutex_) {
    if (absl::MutexLock lock(&cancellation_mutex_); is_cancelled_) {
      return;
    }
    async_operation_();
  }

  /// Calls the current task to be cancelled.
  bool Cancel() ABSL_LOCKS_EXCLUDED(cancellation_mutex_) {
    absl::MutexLock lock(&cancellation_mutex_);
    if (is_cancelled_) {
      return false;
    }

    is_cancelled_ = true;
    return true;
  }

  bool IsCancelled() ABSL_LOCKS_EXCLUDED(cancellation_mutex_) {
    absl::MutexLock lock(&cancellation_mutex_);
    return is_cancelled_;
  }

 private:
  /// Async operation to be executed.
  AsyncOperation async_operation_;

  /**
   * @brief Execution timestamp. A task can be scheduled for the future to be
   * executed.
   */
  Timestamp execution_timestamp_;

  /// Cancellation mutex
  absl::Mutex cancellation_mutex_;

  /// Indicates whether a task was cancelled.
  bool is_cancelled_ ABSL_GUARDED_BY(cancellation_mutex_);
};

/// Comparer class for the AsyncTasks
class AsyncTaskCompareGreater {
 public:
  bool operator()(std::shared_ptr<AsyncTask>& lhs,
                  std::shared_ptr<AsyncTask>& rhs) {
    return lhs->GetExecutionTimestamp() > rhs->GetExecutionTimestamp();
  }
};
}  // namespace google::scp::core

#endif  // CORE_ASYNC_EXECUTOR_ASYNC_TASK_H_
