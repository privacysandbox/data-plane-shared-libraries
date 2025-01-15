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

#include "src/core/async_executor/async_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "absl/container/flat_hash_map.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "src/core/async_executor/error_codes.h"
#include "src/core/async_executor/mock/mock_async_executor_with_internals.h"
#include "src/core/async_executor/typedef.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/test/test_config.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::async_executor::mock::MockAsyncExecutorWithInternals;

namespace google::scp::core::test {

TEST(AsyncExecutorTests, EmptyWorkQueue) { AsyncExecutor executor(1, 10); }

TEST(AsyncExecutorTests, ExceedingQueueCapSchedule) {
  constexpr int kQueueCap = 1;
  MockAsyncExecutorWithInternals executor(1, kQueueCap);
  {
    // Blocking queue with enough work
    executor.Schedule(
        [&] { std::this_thread::sleep_for(std::chrono::seconds(5)); },
        AsyncPriority::Normal);

    // try to push more than the queue can handle
    auto start_time = std::chrono::high_resolution_clock::now();
    while (true) {
      auto result = executor.Schedule([&] {}, AsyncPriority::Normal);

      if (result ==
          RetryExecutionResult(errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP)) {
        break;
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      auto diff = end_time - start_time;
      if (diff > std::chrono::seconds(5)) {
        FAIL() << "Queue cap schedule was never exceeded.";
      }
    }
  }

  {
    AsyncTask task;
    auto two_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                           std::chrono::seconds(2))
                           .count();

    auto schedule_for_timestamp = task.GetExecutionTimestamp() + two_seconds;
    executor.ScheduleFor([&] {}, schedule_for_timestamp);
    auto result = executor.ScheduleFor([&] {}, task.GetExecutionTimestamp());
    EXPECT_THAT(result, ResultIs(RetryExecutionResult(
                            errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP)));
  }
}

TEST(AsyncExecutorTests, CountWorkSingleThread) {
  constexpr int kQueueCap = 10;
  AsyncExecutor executor(1, kQueueCap);
  {
    absl::BlockingCounter count(kQueueCap);
    for (int i = 0; i < kQueueCap; i++) {
      executor.Schedule([&] { count.DecrementCount(); }, AsyncPriority::Normal);
    }
    // Waits some time to finish the work.
    count.Wait();
  }
  {
    absl::BlockingCounter count(kQueueCap);
    for (int i = 0; i < kQueueCap; i++) {
      executor.ScheduleFor([&] { count.DecrementCount(); }, 123456);
    }
    // Waits some time to finish the work.
    count.Wait();
  }
}

TEST(AsyncExecutorTests, CountWorkSingleThreadWithAffinity) {
  constexpr int kQueueCap = 10;
  AsyncExecutor executor(1, kQueueCap);
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap / 2; i++) {
    executor.Schedule(
        [&] {
          auto thread_id = std::this_thread::get_id();
          count.DecrementCount();
          // Schedule another increment - test the affinity path with
          // matching thread IDs.
          executor.Schedule(
              [&count, thread_id = thread_id] {
                // The chosen thread ID should be the same as the calling one.
                EXPECT_EQ(std::this_thread::get_id(), thread_id);
                count.DecrementCount();
              },
              AsyncPriority::Normal,
              AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
        },
        AsyncPriority::Normal,
        AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
  }
  // Waits some time to finish the work.
  count.Wait();
}

TEST(AsyncExecutorTests, CountWorkSingleThreadScheduleForWithAffinity) {
  constexpr int kQueueCap = 10;
  AsyncExecutor executor(1, kQueueCap);
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap / 2; i++) {
    executor.ScheduleFor(
        [&] {
          auto thread_id = std::this_thread::get_id();
          count.DecrementCount();
          // Schedule another incrementation - test the affinity path with
          // matching thread IDs.
          executor.ScheduleFor(
              [&count, thread_id = thread_id] {
                // The chosen thread ID should be the same as the calling one.
                EXPECT_EQ(std::this_thread::get_id(), thread_id);
                count.DecrementCount();
              },
              123456,
              AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
        },
        123456,
        AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
  }
  // Waits some time to finish the work.
  count.Wait();
}

TEST(AsyncExecutorTests, CountWorkSingleThreadNormalToUrgent) {
  constexpr int kQueueCap = 10;
  AsyncExecutor executor(1, kQueueCap);
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap / 2; i++) {
    executor.Schedule(
        [&] {
          count.DecrementCount();
          // Schedule another incrementation - test the affinity path with
          // matching thread IDs.
          executor.ScheduleFor(
              [&] { count.DecrementCount(); }, 123456,
              AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
        },
        AsyncPriority::Normal,
        AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
  }
  // Waits some time to finish the work.
  count.Wait();
}

TEST(AsyncExecutorTests, CountWorkSingleThreadUrgentToNormal) {
  constexpr int kQueueCap = 10;
  AsyncExecutor executor(1, kQueueCap);
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap / 2; i++) {
    executor.ScheduleFor(
        [&] {
          count.DecrementCount();
          // Schedule another incrementation - test the affinity path with
          // matching thread IDs.
          executor.Schedule(
              [&] { count.DecrementCount(); }, AsyncPriority::Normal,
              AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
        },
        123456,
        AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor);
  }
  // Waits some time to finish the work.
  count.Wait();
}

TEST(AsyncExecutorTests, CountWorkMultipleThread) {
  constexpr int kQueueCap = 50;
  AsyncExecutor executor(10, kQueueCap);
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap; i++) {
    executor.Schedule([&] { count.DecrementCount(); }, AsyncPriority::Normal);
  }
  // Waits some time to finish the work.
  count.Wait();
}

TEST(AsyncExecutorTests, AsyncContextCallback) {
  AsyncExecutor executor(1, 10);
  {
    absl::Notification callback_count;
    auto request = std::make_shared<std::string>("request");
    auto callback = [&](AsyncContext<std::string, std::string>& context) {
      callback_count.Notify();
    };
    auto context = AsyncContext<std::string, std::string>(request, callback);

    executor.Schedule(
        [&] {
          context.response = std::make_shared<std::string>("response");
          context.Finish(SuccessExecutionResult());
        },
        AsyncPriority::Normal);

    // Waits some time to finish the work.
    callback_count.WaitForNotification();

    // Verifies the work is executed.
    ASSERT_SUCCESS(context.result);
    EXPECT_EQ(*(context.response), "response");
  }

  {
    // Atomic is not used here because we just reserve one thread in the
    absl::Notification callback_count;
    auto request = std::make_shared<std::string>("request");
    auto callback = [&](AsyncContext<std::string, std::string>& context) {
      callback_count.Notify();
    };
    auto context = AsyncContext<std::string, std::string>(request, callback);

    executor.ScheduleFor(
        [&] {
          context.response = std::make_shared<std::string>("response");
          context.Finish(SuccessExecutionResult());
        },
        12345);

    // Waits some time to finish the work.
    callback_count.WaitForNotification();

    // Verifies the work is executed.
    ASSERT_SUCCESS(context.result);
    EXPECT_EQ(*(context.response), "response");
  }
}

TEST(AsyncExecutorTests, FinishWorkWhenStopInMiddle) {
  constexpr int kQueueCap = 5;
  AsyncExecutor executor(2, kQueueCap);
  absl::Mutex count_mu;
  int normal_count = 0;
  for (int i = 0; i < kQueueCap; i++) {
    executor.Schedule(
        [&] {
          {
            absl::MutexLock lock(&count_mu);
            normal_count++;
          }
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        AsyncPriority::Normal);
  }

  int urgent_count = 0;
  for (int i = 0; i < kQueueCap; i++) {
    executor.Schedule(
        [&] {
          {
            absl::MutexLock lock(&count_mu);
            urgent_count++;
          }
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        AsyncPriority::Urgent);
  }

  // Waits some time to finish the work.
  {
    absl::MutexLock lock(&count_mu);
    auto condition_fn = [&] {
      count_mu.AssertReaderHeld();
      return urgent_count == kQueueCap && normal_count == kQueueCap;
    };
    count_mu.Await(absl::Condition(&condition_fn));
  }

  EXPECT_EQ(urgent_count, kQueueCap);
  EXPECT_EQ(normal_count, kQueueCap);
}

template <typename TExecutor>
void TestPickTaskExecutor(TaskExecutorPoolType pool_type,
                          TaskLoadBalancingScheme scheme,
                          const std::vector<int>& pick_times_list = {10}) {
  AsyncExecutor executor(1, 1);
  std::vector<std::unique_ptr<TExecutor>> task_executor_pool;

  int num_executors =
      std::accumulate(pick_times_list.begin(), pick_times_list.end(), 0);
  task_executor_pool.reserve(num_executors);
  for (int i = 0; i < num_executors; i++) {
    task_executor_pool.push_back(
        std::make_unique<TExecutor>(100 /* queue cap */));
  }

  // Run picking executors
  absl::Mutex task_executor_pool_picked_counts_mu;
  absl::flat_hash_map<TExecutor*, int> task_executor_pool_picked_counts;

  auto picking_function = [&](int pick_times) {
    for (int i = 0; i < pick_times; i++) {
      auto task_executor_or = executor.PickTaskExecutor(
          AsyncExecutorAffinitySetting::NonAffinitized, task_executor_pool,
          pool_type, scheme);
      ASSERT_SUCCESS(task_executor_or);
      absl::MutexLock lock(&task_executor_pool_picked_counts_mu);
      task_executor_pool_picked_counts[*task_executor_or] += 1;
    }
  };

  std::vector<std::thread> threads;
  threads.reserve(pick_times_list.size());
  for (auto pick_times : pick_times_list) {
    threads.emplace_back(picking_function, pick_times);
  }

  for (auto& thread : threads) {
    thread.join();
  }

  bool atleast_one_executor_picked_twice = false;
  bool is_thread_local_concurrent =
      scheme == TaskLoadBalancingScheme::RoundRobinPerThread &&
      pick_times_list.size() > 1;
  size_t total_count = 0;
  for (const auto& task_executor : task_executor_pool) {
    if (is_thread_local_concurrent) {
      if (task_executor_pool_picked_counts[task_executor.get()] >= 2) {
        atleast_one_executor_picked_twice = true;
      }
    } else {
      EXPECT_EQ(task_executor_pool_picked_counts[task_executor.get()], 1);
    }
    total_count += task_executor_pool_picked_counts[task_executor.get()];
  }

  ASSERT_TRUE(
      !is_thread_local_concurrent ||
      (is_thread_local_concurrent && atleast_one_executor_picked_twice));
  ASSERT_EQ(total_count, num_executors);
}

TEST(AsyncExecutorTests, PickTaskExecutorRoundRobinGlobalUrgentPool) {
  TestPickTaskExecutor<SingleThreadPriorityAsyncExecutor>(
      TaskExecutorPoolType::UrgentPool,
      TaskLoadBalancingScheme::RoundRobinGlobal);
}

TEST(AsyncExecutorTests, PickTaskExecutorRoundRobinGlobalNonUrgentPool) {
  TestPickTaskExecutor<SingleThreadAsyncExecutor>(
      TaskExecutorPoolType::NotUrgentPool,
      TaskLoadBalancingScheme::RoundRobinGlobal);
}

TEST(AsyncExecutorTests, PickTaskExecutorRoundRobinThreadLocalUrgentPool) {
  AsyncExecutor executor(1, 1);
  int num_executors = 10;
  std::vector<std::unique_ptr<SingleThreadPriorityAsyncExecutor>>
      task_executor_pool;
  task_executor_pool.reserve(num_executors);
  for (int i = 0; i < num_executors; i++) {
    task_executor_pool.push_back(
        std::make_unique<SingleThreadPriorityAsyncExecutor>(
            100 /* queue cap */));
  }

  // Run picking executors
  absl::flat_hash_map<SingleThreadPriorityAsyncExecutor*, int>
      task_executor_pool_picked_counts;
  for (int i = 0; i < num_executors; i++) {
    auto task_executor_or = executor.PickTaskExecutor(
        AsyncExecutorAffinitySetting::NonAffinitized, task_executor_pool,
        TaskExecutorPoolType::UrgentPool,
        TaskLoadBalancingScheme::RoundRobinPerThread);
    ASSERT_SUCCESS(task_executor_or);
    task_executor_pool_picked_counts[*task_executor_or] += 1;
  }

  // Verify the picked counts are 1 on all the executors
  for (const auto& task_executor : task_executor_pool) {
    EXPECT_EQ(task_executor_pool_picked_counts[task_executor.get()], 1);
  }

  // A different thread picks round robin as well.
  std::thread thread([&] {
    for (int i = 0; i < num_executors; i++) {
      auto task_executor_or = executor.PickTaskExecutor(
          AsyncExecutorAffinitySetting::NonAffinitized, task_executor_pool,
          TaskExecutorPoolType::UrgentPool,
          TaskLoadBalancingScheme::RoundRobinPerThread);
      ASSERT_SUCCESS(task_executor_or);
      task_executor_pool_picked_counts[*task_executor_or] += 1;
    }

    // Verify the picked counts are 2 on all the executors
    for (const auto& task_executor : task_executor_pool) {
      EXPECT_EQ(task_executor_pool_picked_counts[task_executor.get()], 2);
    }
  });
  thread.join();
}

TEST(AsyncExecutorTests, PickTaskExecutorRoundRobinThreadLocalNonUrgentPool) {
  TestPickTaskExecutor<SingleThreadAsyncExecutor>(
      TaskExecutorPoolType::NotUrgentPool,
      TaskLoadBalancingScheme::RoundRobinPerThread);
}

TEST(AsyncExecutorTests, PickTaskExecutorRoundRobinThreadLocalConcurrent) {
  std::vector<int> pick_times = {15, 4, 10, 11};
  TestPickTaskExecutor<SingleThreadAsyncExecutor>(
      TaskExecutorPoolType::NotUrgentPool,
      TaskLoadBalancingScheme::RoundRobinPerThread, pick_times);
}

TEST(AsyncExecutorTests, PickTaskExecutorRoundRobinGlobalConcurrent) {
  std::vector<int> pick_times = {15, 4, 10, 11};
  TestPickTaskExecutor<SingleThreadAsyncExecutor>(
      TaskExecutorPoolType::NotUrgentPool,
      TaskLoadBalancingScheme::RoundRobinGlobal, pick_times);
}

TEST(AsyncExecutorTests, TestPickRandomTaskExecutorWithAffinity) {
  // Picks random executor even with affinity.
  AsyncExecutor async_executor(1, 1);
  std::vector<std::unique_ptr<SingleThreadAsyncExecutor>> task_executor_pool;
  task_executor_pool.push_back(
      std::make_unique<SingleThreadAsyncExecutor>(100 /* queue cap */));
  auto& executor = task_executor_pool.back();
  const auto expected_id = executor->GetThreadId();
  auto chosen_task_executor_or = async_executor.PickTaskExecutor(
      AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor,
      task_executor_pool, TaskExecutorPoolType::NotUrgentPool,
      TaskLoadBalancingScheme::Random);

  ASSERT_SUCCESS(chosen_task_executor_or);

  // Picking an executor should result in a random executor being chosen.
  EXPECT_EQ((*chosen_task_executor_or)->GetThreadId(), expected_id);
}

TEST(AsyncExecutorTests, TestPickNonUrgentToNonUrgentTaskExecutorWithAffinity) {
  // Picks the same non-urgent thread when executing subsequent work.
  AsyncExecutor executor(10, 1);
  // Scheduling another task with affinity should result in using the same
  // thread.
  absl::Notification done;
  std::vector<std::unique_ptr<SingleThreadAsyncExecutor>>
      task_executor_pool;  // unused.
  // Schedule arbitrary work to be done. Using the chosen thread of this work,
  // ensure that picking another executor with affinity chooses this same
  // thread.
  executor.Schedule(
      [&executor, &done, &task_executor_pool] {
        auto chosen_task_executor_or = executor.PickTaskExecutor(
            AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor,
            task_executor_pool, TaskExecutorPoolType::NotUrgentPool,
            TaskLoadBalancingScheme::Random);
        ASSERT_SUCCESS(chosen_task_executor_or);
        // Picking an executor should choose the executor owning the current
        // thread.
        EXPECT_EQ((*chosen_task_executor_or)->GetThreadId(),
                  std::this_thread::get_id());
        done.Notify();
      },
      AsyncPriority::Normal);
  done.WaitForNotification();
}

TEST(AsyncExecutorTests, TestPickNonUrgentToUrgentTaskExecutorWithAffinity) {
  // Picks the urgent thread with the same affinity when executing subsequent
  // work.
  AsyncExecutor executor(10, 1);
  // Scheduling another task with affinity should result in using the same
  // thread.
  absl::Notification done;
  std::vector<std::unique_ptr<SingleThreadPriorityAsyncExecutor>>
      task_executor_pool;  // unused.
  // Schedule arbitrary work to be done. Using the chosen thread of this work,
  // ensure that picking another executor with affinity chooses this same
  // thread.
  executor.Schedule(
      [&executor, &done, &task_executor_pool] {
        auto chosen_task_executor_or = executor.PickTaskExecutor(
            AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor,
            task_executor_pool, TaskExecutorPoolType::UrgentPool,
            TaskLoadBalancingScheme::Random);
        ASSERT_SUCCESS(chosen_task_executor_or);
        // Lookup the corresponding threads in the map.
        const auto& [normal_executor, urgent_executor] =
            executor.GetExecutorForTesting(std::this_thread::get_id());
        EXPECT_EQ(urgent_executor, *chosen_task_executor_or);
        done.Notify();
      },
      AsyncPriority::Normal);
  done.WaitForNotification();
}

TEST(AsyncExecutorTests, TestPickUrgentToUrgentTaskExecutorWithAffinity) {
  // Picks the non-urgent thread with the same affinity when executing
  // subsequent work.
  AsyncExecutor executor(10, 1);
  // Scheduling another task with affinity should result in using the same
  // thread.
  absl::Notification done;
  std::vector<std::unique_ptr<SingleThreadPriorityAsyncExecutor>>
      task_executor_pool;  // unused.
  // Schedule arbitrary work to be done. Using the chosen thread of this work,
  // ensure that picking another executor with affinity chooses this same
  // thread.
  executor.ScheduleFor(
      [&executor, &done, &task_executor_pool] {
        auto chosen_task_executor_or = executor.PickTaskExecutor(
            AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor,
            task_executor_pool, TaskExecutorPoolType::NotUrgentPool,
            TaskLoadBalancingScheme::Random);
        ASSERT_SUCCESS(chosen_task_executor_or);
        // Picking an executor should choose the executor owning the current
        // thread.
        EXPECT_EQ((*chosen_task_executor_or)->GetThreadId(),
                  std::this_thread::get_id());
        done.Notify();
      },
      123456);
  done.WaitForNotification();
}

TEST(AsyncExecutorTests, TestPickUrgentToNonUrgentTaskExecutorWithAffinity) {
  // Picks the non-urgent thread with the same affinity when executing
  // subsequent work.
  AsyncExecutor executor(10, 1);
  // Scheduling another task with affinity should result in using the same
  // thread.
  absl::Notification done;
  std::vector<std::unique_ptr<SingleThreadAsyncExecutor>>
      task_executor_pool;  // unused.
  // Schedule arbitrary work to be done. Using the chosen thread of this work,
  // ensure that picking another executor with affinity chooses this same
  // thread.
  executor.ScheduleFor(
      [&executor, &done, &task_executor_pool] {
        auto chosen_task_executor_or = executor.PickTaskExecutor(
            AsyncExecutorAffinitySetting::AffinitizedToCallingAsyncExecutor,
            task_executor_pool, TaskExecutorPoolType::NotUrgentPool,
            TaskLoadBalancingScheme::Random);
        ASSERT_SUCCESS(chosen_task_executor_or);
        // Lookup the corresponding threads in the map.
        const auto& [normal_executor, urgent_executor] =
            executor.GetExecutorForTesting(std::this_thread::get_id());
        EXPECT_EQ(normal_executor, *chosen_task_executor_or);
        done.Notify();
      },
      123456);
  done.WaitForNotification();
}
}  // namespace google::scp::core::test
