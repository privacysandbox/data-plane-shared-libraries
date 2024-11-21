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

#include "single_thread_priority_async_executor.h"

#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <queue>
#include <thread>
#include <utility>
#include <vector>

#include "absl/time/time.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/public/core/interface/execution_result.h"

#include "async_executor_utils.h"
#include "error_codes.h"
#include "typedef.h"

using google::scp::core::common::TimeProvider;

namespace google::scp::core {

SingleThreadPriorityAsyncExecutor::SingleThreadPriorityAsyncExecutor(
    size_t queue_cap, std::optional<size_t> affinity_cpu_number)
    : is_running_(true),
      worker_thread_started_(false),
      worker_thread_stopped_(false),
      update_wait_time_(false),
      next_scheduled_task_timestamp_(std::numeric_limits<uint64_t>::max()),
      queue_cap_(queue_cap),
      affinity_cpu_number_(affinity_cpu_number) {
  working_thread_.emplace(
      [affinity_cpu_number =
           affinity_cpu_number_](SingleThreadPriorityAsyncExecutor* ptr) {
        if (affinity_cpu_number.has_value()) {
          // Ignore error.
          AsyncExecutorUtils::SetAffinity(*affinity_cpu_number);
        }
        {
          absl::MutexLock lock(&ptr->mutex_);
          ptr->worker_thread_started_ = true;
        }
        ptr->StartWorker();
        {
          absl::MutexLock lock(&ptr->mutex_);
          ptr->worker_thread_stopped_ = true;
        }
      },
      this);
  working_thread_id_ = working_thread_->get_id();
  working_thread_->detach();
}

void SingleThreadPriorityAsyncExecutor::StartWorker() noexcept {
  absl::Duration wait_timeout_duration = absl::InfiniteDuration();
  while (true) {
    std::shared_ptr<AsyncTask> top;
    Timestamp current_timestamp =
        TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
    {
      absl::MutexLock lock(&mutex_);
      auto fn = [this, current_timestamp] {
        mutex_.AssertReaderHeld();
        return !is_running_ || update_wait_time_ ||
               current_timestamp > next_scheduled_task_timestamp_;
      };
      mutex_.AwaitWithTimeout(absl::Condition(&fn), wait_timeout_duration);

      update_wait_time_ = false;

      // Discard any cancelled tasks on top of the queue as an optimization to
      // avoid waiting for the future to arrive on an already cancelled task
      while (!queue_.empty() && queue_.top()->IsCancelled()) {
        queue_.pop();
      }

      if (queue_.size() == 0) {
        if (!is_running_) {
          break;
        }
        next_scheduled_task_timestamp_ = std::numeric_limits<uint64_t>::max();
        wait_timeout_duration = absl::InfiniteDuration();
        continue;
      }

      next_scheduled_task_timestamp_ = queue_.top()->GetExecutionTimestamp();
      wait_timeout_duration = absl::ZeroDuration();
      current_timestamp =
          TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
      if (current_timestamp < next_scheduled_task_timestamp_) {
        wait_timeout_duration = absl::Nanoseconds(
            next_scheduled_task_timestamp_ - current_timestamp);
        continue;
      }
      top = queue_.top();
      queue_.pop();
    }
    top->Execute();
  }
}

SingleThreadPriorityAsyncExecutor::~SingleThreadPriorityAsyncExecutor() {
  absl::MutexLock lock(&mutex_);
  is_running_ = false;

  // To ensure stop can happen cleanly, it is required to wait for the thread to
  // start and exit gracefully. If stop happens before the starting the thread,
  // there is a chance that Stop returns successful but the thread has not been
  // killed.
  auto fn = [this] {
    mutex_.AssertReaderHeld();
    return worker_thread_started_ && worker_thread_stopped_;
  };
  mutex_.Await(absl::Condition(&fn));
}

ExecutionResult SingleThreadPriorityAsyncExecutor::ScheduleFor(
    AsyncOperation work, Timestamp timestamp) noexcept {
  std::function<bool()> cancellation_callback = []() { return false; };
  return ScheduleFor(std::move(work), timestamp, cancellation_callback);
};

ExecutionResult SingleThreadPriorityAsyncExecutor::ScheduleFor(
    AsyncOperation work, Timestamp timestamp,
    std::function<bool()>& cancellation_callback) noexcept {
  absl::MutexLock lock(&mutex_);
  if (queue_.size() >= queue_cap_) {
    return RetryExecutionResult(errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP);
  }
  auto task = std::make_shared<AsyncTask>(std::move(work), timestamp);
  cancellation_callback = [task]() mutable { return task->Cancel(); };
  queue_.push(task);
  if (timestamp < next_scheduled_task_timestamp_) {
    next_scheduled_task_timestamp_ = timestamp;
    update_wait_time_ = true;
  }
  return SuccessExecutionResult();
};

std::thread::id SingleThreadPriorityAsyncExecutor::GetThreadId() const {
  return working_thread_id_;
}

}  // namespace google::scp::core
