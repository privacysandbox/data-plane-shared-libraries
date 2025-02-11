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

#include "async_executor.h"

#include <memory>
#include <random>
#include <thread>
#include <vector>

#include "absl/random/random.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"
#include "typedef.h"

namespace google::scp::core {
AsyncExecutor::AsyncExecutor(size_t thread_count, size_t queue_cap,
                             TaskLoadBalancingScheme task_load_balancing_scheme)
    : thread_count_(std::clamp<size_t>(thread_count, 0, kMaxThreadCount)),
      queue_cap_(std::clamp<size_t>(queue_cap, 0, kMaxQueueCap)),
      task_load_balancing_scheme_(task_load_balancing_scheme) {
  for (size_t i = 0; i < thread_count_; ++i) {
    // TODO We select the CPU affinity just starting at 0 and working our way
    // up. Should we instead randomly assign the CPUs?
    size_t cpu_affinity_number = i % std::thread::hardware_concurrency();
    urgent_task_executor_pool_.push_back(
        std::make_unique<SingleThreadPriorityAsyncExecutor>(
            queue_cap_, cpu_affinity_number));
    normal_task_executor_pool_.push_back(
        std::make_unique<SingleThreadAsyncExecutor>(queue_cap_,
                                                    cpu_affinity_number));
    auto* urgent_executor = urgent_task_executor_pool_.back().get();
    auto* normal_executor = normal_task_executor_pool_.back().get();
    auto normal_thread_id = normal_executor->GetThreadId();
    auto urgent_thread_id = urgent_executor->GetThreadId();

    // We map both thread IDs to the same executors because we can maintain
    // affinity when migrating from normal -> urgent or vice versa. The
    // executors at the same index share the same affinity.
    thread_id_to_executor_map_[normal_thread_id] = {normal_executor,
                                                    urgent_executor};
    thread_id_to_executor_map_[urgent_thread_id] = {normal_executor,
                                                    urgent_executor};
  }
}

template <class TaskExecutorType>
ExecutionResultOr<TaskExecutorType*> AsyncExecutor::PickTaskExecutor(
    AsyncExecutorAffinitySetting affinity,
    const std::vector<std::unique_ptr<TaskExecutorType>>& task_executor_pool,
    TaskExecutorPoolType task_executor_pool_type,
    TaskLoadBalancingScheme task_load_balancing_scheme) const {
  absl::BitGen bitgen;
  // Thread local task counters, initial value of the task counter with a random
  // value so that all the caller threads do not pick the same executor to start
  // with
  static thread_local std::atomic<uint64_t> task_counter_urgent_thread_local(
      absl::Uniform<uint64_t>(bitgen));
  static thread_local std::atomic<uint64_t>
      task_counter_not_urgent_thread_local(absl::Uniform<uint64_t>(bitgen));

  // Global task counters
  static std::atomic<uint64_t> task_counter_urgent(0);
  static std::atomic<uint64_t> task_counter_not_urgent(0);

  if (affinity ==
      AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor) {
    // Get the ID of the current running thread. Use it to pick an executor
    // out of the pool.
    auto found_executors =
        thread_id_to_executor_map_.find(std::this_thread::get_id());
    if (found_executors != thread_id_to_executor_map_.end()) {
      const auto& [normal_executor, urgent_executor] = found_executors->second;
      if constexpr (std::is_same_v<TaskExecutorType, NormalTaskExecutor>) {
        return normal_executor;
      }
      if constexpr (std::is_same_v<TaskExecutorType, UrgentTaskExecutor>) {
        return urgent_executor;
      }
    }
    // Here, we are coming from a thread not on the executor, just choose
    // an executor normally.
  }

  if (task_load_balancing_scheme ==
      TaskLoadBalancingScheme::RoundRobinPerThread) {
    if (task_executor_pool_type == TaskExecutorPoolType::UrgentPool) {
      auto picked_index = task_counter_urgent_thread_local.fetch_add(
                              1, std::memory_order_relaxed) %
                          task_executor_pool.size();
      return task_executor_pool.at(picked_index).get();
    } else if (task_executor_pool_type == TaskExecutorPoolType::NotUrgentPool) {
      auto picked_index = task_counter_not_urgent_thread_local.fetch_add(
                              1, std::memory_order_relaxed) %
                          task_executor_pool.size();
      return task_executor_pool.at(picked_index).get();
    } else {
      return FailureExecutionResult(
          errors::SC_ASYNC_EXECUTOR_INVALID_TASK_POOL_TYPE);
    }
  }

  if (task_load_balancing_scheme == TaskLoadBalancingScheme::Random) {
    auto picked_index =
        absl::Uniform<uint64_t>(bitgen) % task_executor_pool.size();
    return task_executor_pool.at(picked_index).get();
  }

  if (task_load_balancing_scheme == TaskLoadBalancingScheme::RoundRobinGlobal) {
    if (task_executor_pool_type == TaskExecutorPoolType::UrgentPool) {
      auto picked_index =
          task_counter_urgent.fetch_add(1) % task_executor_pool.size();
      return task_executor_pool.at(picked_index).get();
    } else if (task_executor_pool_type == TaskExecutorPoolType::NotUrgentPool) {
      auto picked_index =
          task_counter_not_urgent.fetch_add(1) % task_executor_pool.size();
      return task_executor_pool.at(picked_index).get();
    } else {
      return FailureExecutionResult(
          errors::SC_ASYNC_EXECUTOR_INVALID_TASK_POOL_TYPE);
    }
  }

  return FailureExecutionResult(
      errors::SC_ASYNC_EXECUTOR_INVALID_LOAD_BALANCING_TYPE);
}

std::pair<SingleThreadAsyncExecutor*, SingleThreadPriorityAsyncExecutor*>
AsyncExecutor::GetExecutorForTesting(const std::thread::id& id) const {
  if (auto it = thread_id_to_executor_map_.find(id);
      it != thread_id_to_executor_map_.end()) {
    return it->second;
  }
  return {nullptr, nullptr};
}

ExecutionResult AsyncExecutor::Schedule(AsyncOperation work,
                                        AsyncPriority priority) noexcept {
  return Schedule(std::move(work), priority,
                  AsyncExecutorAffinitySetting::NonAffinitized);
}

ExecutionResult AsyncExecutor::Schedule(
    AsyncOperation work, AsyncPriority priority,
    AsyncExecutorAffinitySetting affinity) noexcept {
  if (urgent_task_executor_pool_.size() < thread_count_ ||
      normal_task_executor_pool_.size() < thread_count_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }
  if (priority == AsyncPriority::Urgent) {
    ASSIGN_OR_RETURN(UrgentTaskExecutor * task_executor,
                     PickTaskExecutor(affinity, urgent_task_executor_pool_,
                                      TaskExecutorPoolType::UrgentPool,
                                      task_load_balancing_scheme_));
    return task_executor->ScheduleFor(
        std::move(work),
        common::TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks());
  }
  if (priority == AsyncPriority::Normal || priority == AsyncPriority::High) {
    ASSIGN_OR_RETURN(NormalTaskExecutor * task_executor,
                     PickTaskExecutor(affinity, normal_task_executor_pool_,
                                      TaskExecutorPoolType::NotUrgentPool,
                                      task_load_balancing_scheme_));
    return task_executor->Schedule(std::move(work), priority);
  }
  return FailureExecutionResult(
      errors::SC_ASYNC_EXECUTOR_INVALID_PRIORITY_TYPE);
}

ExecutionResult AsyncExecutor::ScheduleFor(AsyncOperation work,
                                           Timestamp timestamp) noexcept {
  return ScheduleFor(std::move(work), timestamp,
                     AsyncExecutorAffinitySetting::NonAffinitized);
}

ExecutionResult AsyncExecutor::ScheduleFor(
    AsyncOperation work, Timestamp timestamp,
    AsyncExecutorAffinitySetting affinity) noexcept {
  TaskCancellationLambda cancellation_callback = {};
  return ScheduleFor(std::move(work), timestamp, cancellation_callback,
                     affinity);
}

ExecutionResult AsyncExecutor::ScheduleFor(
    AsyncOperation work, Timestamp timestamp,
    TaskCancellationLambda& cancellation_callback) noexcept {
  return ScheduleFor(std::move(work), timestamp, cancellation_callback,
                     AsyncExecutorAffinitySetting::NonAffinitized);
}

ExecutionResult AsyncExecutor::ScheduleFor(
    AsyncOperation work, Timestamp timestamp,
    TaskCancellationLambda& cancellation_callback,
    AsyncExecutorAffinitySetting affinity) noexcept {
  if (urgent_task_executor_pool_.size() < thread_count_) {
    return FailureExecutionResult(errors::SC_ASYNC_EXECUTOR_NOT_INITIALIZED);
  }
  ASSIGN_OR_RETURN(UrgentTaskExecutor * task_executor,
                   PickTaskExecutor(affinity, urgent_task_executor_pool_,
                                    TaskExecutorPoolType::UrgentPool,
                                    task_load_balancing_scheme_));
  return task_executor->ScheduleFor(std::move(work), timestamp,
                                    cancellation_callback);
}
}  // namespace google::scp::core
