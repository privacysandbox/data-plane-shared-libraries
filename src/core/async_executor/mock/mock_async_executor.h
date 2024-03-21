/*
 * Copyright 2022 Google LLC
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

#ifndef CORE_ASYNC_EXECUTOR_MOCK_MOCK_ASYNC_EXECUTOR_H_
#define CORE_ASYNC_EXECUTOR_MOCK_MOCK_ASYNC_EXECUTOR_H_

#include <functional>
#include <memory>
#include <utility>

#include "src/core/interface/async_executor_interface.h"

namespace google::scp::core::async_executor::mock {
class MockAsyncExecutor : public core::AsyncExecutorInterface {
 public:
  MockAsyncExecutor() {}

  ExecutionResult Schedule(AsyncOperation work,
                           AsyncPriority priority) noexcept override {
    if (schedule_mock) {
      return schedule_mock(std::move(work));
    }

    work();
    return SuccessExecutionResult();
  }

  ExecutionResult Schedule(AsyncOperation work, AsyncPriority priority,
                           AsyncExecutorAffinitySetting) noexcept override {
    return Schedule(std::move(work), priority);
  }

  ExecutionResult ScheduleFor(AsyncOperation work,
                              Timestamp timestamp) noexcept override {
    if (schedule_for_mock) {
      std::function<bool()> callback;
      return schedule_for_mock(std::move(work), timestamp, callback);
    }

    work();
    return SuccessExecutionResult();
  }

  ExecutionResult ScheduleFor(AsyncOperation work, Timestamp timestamp,
                              AsyncExecutorAffinitySetting) noexcept override {
    return ScheduleFor(std::move(work), timestamp);
  }

  ExecutionResult ScheduleFor(
      AsyncOperation work, Timestamp timestamp,
      std::function<bool()>& cancellation_callback) noexcept override {
    if (schedule_for_mock) {
      return schedule_for_mock(std::move(work), timestamp,
                               cancellation_callback);
    }

    work();
    return SuccessExecutionResult();
  }

  ExecutionResult ScheduleFor(AsyncOperation work, Timestamp timestamp,
                              std::function<bool()>& cancellation_callback,
                              AsyncExecutorAffinitySetting) noexcept override {
    return ScheduleFor(std::move(work), timestamp, cancellation_callback);
  }

  std::function<ExecutionResult(AsyncOperation work)> schedule_mock;
  std::function<ExecutionResult(AsyncOperation work, Timestamp,
                                std::function<bool()>&)>
      schedule_for_mock;
};
}  // namespace google::scp::core::async_executor::mock

#endif  // CORE_ASYNC_EXECUTOR_MOCK_MOCK_ASYNC_EXECUTOR_H_
