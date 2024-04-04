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

#include "src/core/common/operation_dispatcher/operation_dispatcher.h"

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <functional>
#include <limits>
#include <memory>
#include <string>

#include "absl/synchronization/notification.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/common/operation_dispatcher/error_codes.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/streaming_context.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::AsyncContext;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::test::ResultIs;

namespace google::scp::core::common::test {
TEST(OperationDispatcherTests, SuccessfulOperation) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_SUCCESS(context.result);
    condition.Notify();
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, SuccessfulOperationProducerStreaming) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  ProducerStreamingContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_SUCCESS(context.result);
    condition.Notify();
  };

  std::function<ExecutionResult(
      ProducerStreamingContext<std::string, std::string>&)>
      dispatch_to_component =
          [](ProducerStreamingContext<std::string, std::string>& context) {
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          };

  dispatcher.DispatchProducerStreaming(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, SuccessfulOperationConsumerStreaming) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  std::atomic<int> process_call_count(0);
  absl::Notification condition;
  ConsumerStreamingContext<std::string, std::string> context;
  context.process_callback =
      [&](ConsumerStreamingContext<std::string, std::string>& context,
          bool is_finish) {
        if (is_finish) {
          EXPECT_SUCCESS(context.result);
          condition.Notify();
        } else {
          process_call_count++;
        }
      };

  std::function<ExecutionResult(
      ConsumerStreamingContext<std::string, std::string>&)>
      dispatch_to_component =
          [](ConsumerStreamingContext<std::string, std::string>& context) {
            context.ProcessNextMessage();
            context.ProcessNextMessage();
            context.Finish(SuccessExecutionResult());
            return SuccessExecutionResult();
          };

  dispatcher.DispatchConsumerStreaming(context, dispatch_to_component);
  condition.WaitForNotification();
  // Expect it to be called twice - once per ProcessNextMessage call.
  EXPECT_EQ(process_call_count, 2);
}

TEST(OperationDispatcherTests, FailedOperation) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
    condition.Notify();
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            context.Finish(FailureExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, FailedOperationProducerStreaming) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  ProducerStreamingContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
    condition.Notify();
  };

  std::function<ExecutionResult(
      ProducerStreamingContext<std::string, std::string>&)>
      dispatch_to_component =
          [](ProducerStreamingContext<std::string, std::string>& context) {
            context.Finish(FailureExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.DispatchProducerStreaming(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, FailedOperationConsumerStreaming) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  std::atomic<int> process_call_count(0);
  absl::Notification condition;
  ConsumerStreamingContext<std::string, std::string> context;
  context.process_callback =
      [&](ConsumerStreamingContext<std::string, std::string>& context,
          bool is_finish) {
        if (is_finish) {
          EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1)));
          condition.Notify();
        } else {
          process_call_count++;
        }
      };

  std::function<ExecutionResult(
      ConsumerStreamingContext<std::string, std::string>&)>
      dispatch_to_component =
          [](ConsumerStreamingContext<std::string, std::string>& context) {
            context.ProcessNextMessage();
            context.ProcessNextMessage();
            context.Finish(FailureExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.DispatchConsumerStreaming(context, dispatch_to_component);
  condition.WaitForNotification();
  // Expect it to be called twice - once per ProcessNextMessage call.
  EXPECT_EQ(process_call_count, 2);
}

TEST(OperationDispatcherTests, RetryOperation) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    EXPECT_EQ(context.retry_count, 5);
    condition.Notify();
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            context.Finish(RetryExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, RetryOperationProducerStreaming) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  ProducerStreamingContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    EXPECT_EQ(context.retry_count, 5);
    condition.Notify();
  };

  std::function<ExecutionResult(
      ProducerStreamingContext<std::string, std::string>&)>
      dispatch_to_component =
          [](ProducerStreamingContext<std::string, std::string>& context) {
            context.Finish(RetryExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.DispatchProducerStreaming(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, RetryOperationConsumerStreaming) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  std::atomic<int> process_call_count(0);
  absl::Notification condition;
  ConsumerStreamingContext<std::string, std::string> context;
  context.process_callback =
      [&](ConsumerStreamingContext<std::string, std::string>& context,
          bool is_finish) {
        if (is_finish) {
          EXPECT_THAT(context.result,
                      ResultIs(FailureExecutionResult(
                          core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
          EXPECT_EQ(context.retry_count, 5);
          condition.Notify();
        } else {
          process_call_count++;
        }
      };

  std::function<ExecutionResult(
      ConsumerStreamingContext<std::string, std::string>&)>
      dispatch_to_component =
          [](ConsumerStreamingContext<std::string, std::string>& context) {
            context.ProcessNextMessage();
            context.ProcessNextMessage();
            context.Finish(RetryExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.DispatchConsumerStreaming(context, dispatch_to_component);
  condition.WaitForNotification();
  // Expect 2 calls per try.
  int expected_call_count = 2 * retry_strategy.GetMaximumAllowedRetryCount();
  EXPECT_EQ(process_call_count, expected_call_count);
}

TEST(OperationDispatcherTests, OperationExpiration) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 10, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  AsyncContext<std::string, std::string> context;
  context.expiration_time = std::numeric_limits<uint64_t>::max();

  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_OPERATION_EXPIRED)));
    EXPECT_EQ(context.retry_count, 4);
    condition.Notify();
  };

  std::atomic<size_t> retry_count = 0;
  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [&](AsyncContext<std::string, std::string>& context) {
            if (++retry_count == 4) {
              context.expiration_time = 1234;
            }
            context.Finish(RetryExecutionResult(1));
            return SuccessExecutionResult();
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, FailedOnAcceptance) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result, ResultIs(FailureExecutionResult(1234)));
    condition.Notify();
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            return FailureExecutionResult(1234);
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  condition.WaitForNotification();
}

TEST(OperationDispatcherTests, RetryOnAcceptance) {
  MockAsyncExecutor mock_async_executor;
  RetryStrategy retry_strategy(RetryStrategyType::Exponential, 0, 5);
  OperationDispatcher dispatcher(&mock_async_executor, retry_strategy);

  absl::Notification condition;
  AsyncContext<std::string, std::string> context;
  context.callback = [&](AsyncContext<std::string, std::string>& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    core::errors::SC_DISPATCHER_EXHAUSTED_RETRIES)));
    condition.Notify();
  };

  std::function<ExecutionResult(AsyncContext<std::string, std::string>&)>
      dispatch_to_component =
          [](AsyncContext<std::string, std::string>& context) {
            return RetryExecutionResult(1234);
          };

  dispatcher.Dispatch(context, dispatch_to_component);
  condition.WaitForNotification();
}

}  // namespace google::scp::core::common::test
