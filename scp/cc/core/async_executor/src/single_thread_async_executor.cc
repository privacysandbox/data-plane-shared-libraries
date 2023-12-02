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

#include "single_thread_async_executor.h"

#include <chrono>
#include <functional>
#include <memory>
#include <thread>
#include <utility>

#include "async_executor_utils.h"
#include "error_codes.h"
#include "typedef.h"

using google::scp::core::common::ConcurrentQueue;

namespace {
constexpr absl::Duration kLockWaitTime = absl::Milliseconds(5);
}  // namespace

namespace google::scp::core {
ExecutionResult SingleThreadAsyncExecutor::Init() noexcept {
  if (queue_cap_ <= 0 || queue_cap_ > kMaxQueueCap) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_INVALID_QUEUE_CAP);
  }
  absl::MutexLock l(&mutex_);
  normal_pri_queue_.emplace(queue_cap_);
  high_pri_queue_.emplace(queue_cap_);
  return SuccessExecutionResult();
};

ExecutionResult SingleThreadAsyncExecutor::Run() noexcept {
  absl::MutexLock l(&mutex_);
  if (is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_ALREADY_RUNNING);
  }
  if (!normal_pri_queue_ || !high_pri_queue_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }
  is_running_ = true;
  working_thread_.emplace(
      [affinity_cpu_number =
           affinity_cpu_number_](SingleThreadAsyncExecutor* ptr) {
        if (affinity_cpu_number.has_value()) {
          // Ignore error.
          AsyncExecutorUtils::SetAffinity(*affinity_cpu_number);
        }
        {
          absl::MutexLock l(&ptr->mutex_);
          ptr->worker_thread_started_ = true;
        }
        ptr->StartWorker();
        {
          absl::MutexLock l(&ptr->mutex_);
          ptr->worker_thread_stopped_ = true;
        }
      },
      this);
  working_thread_id_ = working_thread_->get_id();
  working_thread_->detach();
  return SuccessExecutionResult();
}

void SingleThreadAsyncExecutor::StartWorker() noexcept {
  while (true) {
    std::unique_ptr<AsyncTask> task;
    {
      absl::MutexLock l(&mutex_);
      auto fn = [this] {
        mutex_.AssertReaderHeld();
        return !is_running_ || high_pri_queue_->Size() > 0 ||
               normal_pri_queue_->Size() > 0;
      };
      mutex_.AwaitWithTimeout(absl::Condition(&fn), kLockWaitTime);

      if (normal_pri_queue_->Size() == 0 && high_pri_queue_->Size() == 0) {
        if (!is_running_) {
          break;
        }
        continue;
      }

      // The priority is with the high pri tasks.
      if (!high_pri_queue_->TryDequeue(task).Successful() &&
          !normal_pri_queue_->TryDequeue(task).Successful()) {
        continue;
      }
    }
    task->Execute();
  }
}

ExecutionResult SingleThreadAsyncExecutor::Stop() noexcept {
  absl::MutexLock l(&mutex_);
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }
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
  return SuccessExecutionResult();
};

ExecutionResult SingleThreadAsyncExecutor::Schedule(
    AsyncOperation work, AsyncPriority priority) noexcept {
  absl::MutexLock l(&mutex_);
  if (!is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }
  if (priority != AsyncPriority::Normal && priority != AsyncPriority::High) {
    return FailureExecutionResult(
        errors::SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE);
  }

  auto task = std::make_unique<AsyncTask>(std::move(work));
  ExecutionResult execution_result;
  if (priority == AsyncPriority::Normal) {
    execution_result = normal_pri_queue_->TryEnqueue(std::move(task));
  } else {
    execution_result = high_pri_queue_->TryEnqueue(std::move(task));
  }
  if (!execution_result.Successful()) {
    return RetryExecutionResult(errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP);
  }
  return SuccessExecutionResult();
};

ExecutionResultOr<std::thread::id> SingleThreadAsyncExecutor::GetThreadId()
    const {
  if (absl::MutexLock l(&mutex_); !is_running_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_RUNNING);
  }
  return working_thread_id_;
}

}  // namespace google::scp::core
