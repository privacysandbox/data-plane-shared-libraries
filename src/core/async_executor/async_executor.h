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

#ifndef CORE_ASYNC_EXECUTOR_ASYNC_EXECUTOR_H_
#define CORE_ASYNC_EXECUTOR_ASYNC_EXECUTOR_H_

#include <memory>
#include <thread>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "async_task.h"
#include "error_codes.h"
#include "single_thread_async_executor.h"
#include "single_thread_priority_async_executor.h"

namespace google::scp::core {

/**
 * @brief Scheme to spread incoming tasks on to executor pool workers
 */
enum class TaskLoadBalancingScheme {
  /**
   * @brief Round Robin across the executors
   */
  RoundRobinGlobal = 0,
  /**
   * @brief Loosely Round Robin w.r.t thread local state
   */
  RoundRobinPerThread = 1,
  /**
   * @brief Random across the executors
   */
  Random = 2
};

/**
 * @brief Pool types for task executors.
 * NOTE: Any new thread pool added to TaskExecutor should be reflected here
 * accordingly
 */
enum class TaskExecutorPoolType { UrgentPool = 0, NotUrgentPool = 1 };

/*! @copydoc AsyncExecutorInterface
 */
class AsyncExecutor : public AsyncExecutorInterface {
 public:
  /**
   * @brief Construct a new Async Executor object with given thread_count and
   * queue_cap.
   *
   * @param thread_count the number of threads in the pool.
   * @param queue_cap the maximum size of each work queue. It is not an accurate
   * cap due to the concurrency of the queue.
   * @param drop_tasks_on_stop indicates whether the executor should wait on
   * the tasks during the stop operation.
   * @param task_load_balancing_scheme indicates the type of load balancing
   * scheme to use for the tasks
   */
  AsyncExecutor(size_t thread_count, size_t queue_cap,
                TaskLoadBalancingScheme task_load_balancing_scheme =
                    TaskLoadBalancingScheme::RoundRobinGlobal);

  ExecutionResult Schedule(AsyncOperation work,
                           AsyncPriority priority) noexcept override;

  ExecutionResult Schedule(
      AsyncOperation work, AsyncPriority priority,
      AsyncExecutorAffinitySetting affinity) noexcept override;

  ExecutionResult ScheduleFor(AsyncOperation work,
                              Timestamp timestamp) noexcept override;

  ExecutionResult ScheduleFor(
      AsyncOperation work, Timestamp timestamp,
      AsyncExecutorAffinitySetting affinity) noexcept override;

  ExecutionResult ScheduleFor(
      AsyncOperation work, Timestamp timestamp,
      TaskCancellationLambda& cancellation_callback) noexcept override;

  ExecutionResult ScheduleFor(
      AsyncOperation work, Timestamp timestamp,
      TaskCancellationLambda& cancellation_callback,
      AsyncExecutorAffinitySetting affinity) noexcept override;

  // Pointer is non-null if status is ok.
  template <class TaskExecutorType>
  ExecutionResultOr<TaskExecutorType*> PickTaskExecutor(
      AsyncExecutorAffinitySetting affinity,
      const std::vector<std::unique_ptr<TaskExecutorType>>& task_executor_pool,
      TaskExecutorPoolType task_executor_pool_type,
      TaskLoadBalancingScheme task_load_balancing_scheme) const;

  std::pair<SingleThreadAsyncExecutor*, SingleThreadPriorityAsyncExecutor*>
  GetExecutorForTesting(const std::thread::id& id) const;

 protected:
  using UrgentTaskExecutor = SingleThreadPriorityAsyncExecutor;
  using NormalTaskExecutor = SingleThreadAsyncExecutor;

  /// Number of threads in the thread pool.
  size_t thread_count_;
  /// The maximum length of the work queue.
  size_t queue_cap_;
  /// Executor pool for urgent work.
  std::vector<std::unique_ptr<UrgentTaskExecutor>> urgent_task_executor_pool_;
  /// Executor pool for normal work.
  std::vector<std::unique_ptr<NormalTaskExecutor>> normal_task_executor_pool_;
  /// A map of (normal executor and urgent executor) thread IDs and their
  /// corresponding executors (the normal executor and then the urgent executor
  /// with the same affinity).
  absl::flat_hash_map<std::thread::id,
                      std::pair<NormalTaskExecutor*, UrgentTaskExecutor*>>
      thread_id_to_executor_map_;
  /// Load balancing scheme to distribute incoming tasks on to the thread pool
  /// threads.
  TaskLoadBalancingScheme task_load_balancing_scheme_;

 private:
  static constexpr std::string_view kAsyncExecutor = "AsyncExecutor";
};
}  // namespace google::scp::core

#endif  // CORE_ASYNC_EXECUTOR_ASYNC_EXECUTOR_H_
