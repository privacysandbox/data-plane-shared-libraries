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

#ifndef CORE_ASYNC_EXECUTOR_AWS_AWS_ASYNC_EXECUTOR_H_
#define CORE_ASYNC_EXECUTOR_AWS_AWS_ASYNC_EXECUTOR_H_

#include <functional>
#include <memory>

#include <aws/core/utils/threading/Executor.h>

#include "src/core/interface/async_executor_interface.h"

namespace google::scp::core::async_executor::aws {
/**
 * @brief Custom IO (Input/Output) task async executor the AWS library. The
 * current executor implemented in the AWS SDK uses a singled queue with locks
 * which does not provide high throughput as needed. This is a wrapper around
 * the async executor to redirect all the requests to the SCP AsyncExecutor for
 * IO operations.
 */
class AwsAsyncExecutor : public Aws::Utils::Threading::Executor {
 public:
  AwsAsyncExecutor(core::AsyncExecutorInterface* io_async_executor,
                   AsyncPriority io_async_execution_priority =
                       kDefaultAsyncPriorityForBlockingIOTaskExecution)
      : io_async_executor_(io_async_executor),
        io_async_execution_priority_(io_async_execution_priority) {}

 protected:
  bool SubmitToThread(std::function<void()>&& task) override {
    if (io_async_executor_
            ->Schedule([task]() { task(); }, io_async_execution_priority_)
            .Successful()) {
      return true;
    }
    return false;
  }

 private:
  core::AsyncExecutorInterface* io_async_executor_;

  AsyncPriority io_async_execution_priority_;
};
}  // namespace google::scp::core::async_executor::aws

#endif  // CORE_ASYNC_EXECUTOR_AWS_AWS_ASYNC_EXECUTOR_H_
