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

#include "src/core/async_executor/single_thread_priority_async_executor.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <functional>
#include <string>
#include <thread>

#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/error_codes.h"
#include "src/core/async_executor/typedef.h"
#include "src/core/common/time_provider/time_provider.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/test/test_config.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::common::TimeProvider;
using testing::Values;

namespace google::scp::core::test {
TEST(SingleThreadPriorityAsyncExecutorTests, ExceedingQueueCapSchedule) {
  constexpr int kQueueCap = 1;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap);
  AsyncTask task;
  auto two_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::seconds(2))
                         .count();

  auto schedule_for_timestamp = task.GetExecutionTimestamp() + two_seconds;
  ASSERT_SUCCESS(executor.ScheduleFor([&] {}, schedule_for_timestamp));
  auto result = executor.ScheduleFor([&] {}, task.GetExecutionTimestamp());
  EXPECT_THAT(result, ResultIs(RetryExecutionResult(
                          errors::SC_ASYNC_EXECUTOR_EXCEEDING_QUEUE_CAP)));
}

TEST(SingleThreadPriorityAsyncExecutorTests, CountWorkSingleThread) {
  constexpr int kQueueCap = 10;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap);
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap; i++) {
    ASSERT_SUCCESS(
        executor.ScheduleFor([&] { count.DecrementCount(); }, 123456));
  }
  count.Wait();
}

class AffinityTest : public testing::TestWithParam<size_t> {
 protected:
  size_t GetCpu() const { return GetParam(); }
};

TEST_P(AffinityTest, CountWorkSingleThreadWithAffinity) {
  constexpr int kQueueCap = 10;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap, GetCpu());
  absl::BlockingCounter count(kQueueCap);
  for (int i = 0; i < kQueueCap; i++) {
    ASSERT_SUCCESS(executor.ScheduleFor(
        [&] {
          cpu_set_t cpuset;
          CPU_ZERO(&cpuset);
          pthread_getaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
          if (GetCpu() < std::thread::hardware_concurrency()) {
            EXPECT_NE(CPU_ISSET(GetCpu(), &cpuset), 0);
          }
          count.DecrementCount();
        },
        123456));
  }
  count.Wait();
}

TEST(SingleThreadPriorityAsyncExecutorTests,
     HandlesTaskInQueueAfterIdlePeriod) {
  SingleThreadPriorityAsyncExecutor executor(1);
  // Represents an idle time for the server (nothing calls ScheduleFor):
  std::this_thread::sleep_for(std::chrono::seconds(1));

  int64_t half_second = absl::ToInt64Nanoseconds(absl::Milliseconds(500));
  int64_t two_seconds = absl::ToInt64Nanoseconds(absl::Seconds(2));

  const Timestamp scheduled_for =
      TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks() + two_seconds;

  absl::BlockingCounter count(1);
  ASSERT_SUCCESS(executor.ScheduleFor(
      [&count, &scheduled_for, &half_second] {
        const Timestamp executed_at =
            TimeProvider::GetSteadyTimestampInNanosecondsAsClockTicks();
        EXPECT_LT(executed_at - scheduled_for, half_second);
        count.DecrementCount();
      },
      scheduled_for));
  count.Wait();
}

// The test should work for any value, even an invalid CPU #.
INSTANTIATE_TEST_SUITE_P(SingleThreadPriorityAsyncExecutorTests, AffinityTest,
                         Values(0, 1, std::thread::hardware_concurrency() - 1,
                                std::thread::hardware_concurrency()));

TEST(SingleThreadPriorityAsyncExecutorTests, OrderedTasksExecution) {
  constexpr int kQueueCap = 10;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap);
  AsyncTask task;
  auto half_second = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::milliseconds(500))
                         .count();
  auto one_second = std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::seconds(1))
                        .count();
  auto two_seconds = std::chrono::duration_cast<std::chrono::nanoseconds>(
                         std::chrono::seconds(2))
                         .count();

  absl::Mutex counter_mu;
  size_t counter = 0;
  ASSERT_SUCCESS(executor.ScheduleFor(
      [&] {
        absl::MutexLock lock(&counter_mu);
        EXPECT_EQ(counter++, 2);
      },
      task.GetExecutionTimestamp() + two_seconds));
  ASSERT_SUCCESS(executor.ScheduleFor(
      [&] {
        absl::MutexLock lock(&counter_mu);
        EXPECT_EQ(counter++, 1);
      },
      task.GetExecutionTimestamp() + one_second));
  ASSERT_SUCCESS(executor.ScheduleFor(
      [&] {
        absl::MutexLock lock(&counter_mu);
        EXPECT_EQ(counter++, 0);
      },
      task.GetExecutionTimestamp() + half_second));

  {
    absl::MutexLock lock(&counter_mu);
    auto condition_fn = [&] {
      counter_mu.AssertReaderHeld();
      return counter == 3;
    };
    ASSERT_TRUE(counter_mu.AwaitWithTimeout(absl::Condition(&condition_fn),
                                            absl::Seconds(30)));
  }
}

TEST(SingleThreadPriorityAsyncExecutorTests, AsyncContextCallback) {
  SingleThreadPriorityAsyncExecutor executor(10);
  // Atomic is not used here because we just reserve one thread in the
  absl::Notification callback_count;
  auto request = std::make_shared<std::string>("request");
  auto callback = [&](AsyncContext<std::string, std::string>& context) {
    callback_count.Notify();
  };
  auto context = AsyncContext<std::string, std::string>(request, callback);

  ASSERT_SUCCESS(executor.ScheduleFor(
      [&] {
        context.response = std::make_shared<std::string>("response");
        context.Finish(SuccessExecutionResult());
      },
      12345));

  // Waits some time to finish the work.
  ASSERT_TRUE(callback_count.WaitForNotificationWithTimeout(absl::Seconds(30)));

  // Verifies the work is executed.
  EXPECT_EQ(*(context.response), "response");
  ASSERT_SUCCESS(context.result);
  // Verifies the callback is executed.
  EXPECT_TRUE(callback_count.HasBeenNotified());
}

TEST(SingleThreadPriorityAsyncExecutorTests, FinishWorkWhenStopInMiddle) {
  constexpr int kQueueCap = 5;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap);
  absl::BlockingCounter urgent_count(kQueueCap);
  for (int i = 0; i < kQueueCap; i++) {
    ASSERT_SUCCESS(executor.ScheduleFor(
        [&] {
          urgent_count.DecrementCount();
          std::this_thread::sleep_for(UNIT_TEST_SHORT_SLEEP_MS);
        },
        1234));
  }

  // Waits some time to finish the work.
  urgent_count.Wait();
}

TEST(SingleThreadPriorityAsyncExecutorTests, TaskCancellation) {
  constexpr int kQueueCap = 3;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap);
  for (int i = 0; i < kQueueCap; i++) {
    std::function<bool()> cancellation_callback;
    Timestamp next_clock = (TimeProvider::GetSteadyTimestampInNanoseconds() +
                            std::chrono::milliseconds(500))
                               .count();

    ASSERT_SUCCESS(executor.ScheduleFor([&] { EXPECT_EQ(true, false); },
                                        next_clock, cancellation_callback));

    EXPECT_EQ(cancellation_callback(), true);
  }
  std::this_thread::sleep_for(std::chrono::seconds(2));
}

TEST(SingleThreadPriorityAsyncExecutorTests,
     DuringStopDoNotWaitOnCancelledTaskExecutionTimeToArrive) {
  constexpr int kQueueCap = 3;
  SingleThreadPriorityAsyncExecutor executor(kQueueCap);
  for (int i = 0; i < kQueueCap; i++) {
    std::function<bool()> cancellation_callback;
    auto far_ahead_timestamp =
        (TimeProvider::GetSteadyTimestampInNanoseconds() +
         std::chrono::hours(24))
            .count();

    ASSERT_SUCCESS(executor.ScheduleFor([&] { EXPECT_EQ(true, false); },
                                        far_ahead_timestamp,
                                        cancellation_callback));

    // Cancel the task
    EXPECT_EQ(cancellation_callback(), true);
  }
}

}  // namespace google::scp::core::test
