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

#include "src/core/message_router/message_router.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <memory>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "google/protobuf/any.pb.h"
#include "src/core/common/concurrent_queue/concurrent_queue.h"
#include "src/core/interface/async_context.h"
#include "src/core/message_router/error_codes.h"
#include "src/core/message_router/test.pb.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using core::message_router::test::TestBoolRequest;
using core::message_router::test::TestBoolResponse;
using core::message_router::test::TestStringRequest;
using core::message_router::test::TestStringResponse;
using google::protobuf::Any;
using google::scp::core::common::ConcurrentQueue;
using testing::NotNull;

namespace google::scp::core::test {
class MessageRouterTest : public ::testing::Test {
 protected:
  void SetUp() override {
    EXPECT_SUCCESS(router_.Init());
    EXPECT_SUCCESS(router_.Run());

    {
      absl::MutexLock lock(&running_mu_);
      running_ = true;
    }
    thread_ = std::thread([this] {
      auto is_running = [this] {
        absl::MutexLock lock(&running_mu_);
        return running_;
      };
      while (is_running()) {
        std::shared_ptr<AsyncContext<Any, Any>> context;
        auto dequeue_result = queue_->TryDequeue(context);
        if (dequeue_result.Successful()) {
          router_.OnMessageReceived(context);
        } else {
          std::this_thread::yield();
        }
      }
    });

    TestStringRequest test_request_1;
    any_request_1_.PackFrom(test_request_1);
    TestBoolRequest test_request_2;
    any_request_2_.PackFrom(test_request_2);
  }

  void TearDown() override {
    {
      absl::MutexLock lock(&running_mu_);
      running_ = false;
    }
    thread_.join();
    EXPECT_SUCCESS(router_.Stop());
  }

  std::shared_ptr<ConcurrentQueue<std::shared_ptr<AsyncContext<Any, Any>>>>
      queue_ = std::make_shared<
          ConcurrentQueue<std::shared_ptr<AsyncContext<Any, Any>>>>(10);
  MessageRouter router_;
  Any any_request_1_;
  Any any_request_2_;
  std::thread thread_;
  absl::Mutex running_mu_;
  bool running_ ABSL_GUARDED_BY(running_mu_);
};

TEST_F(MessageRouterTest, RequestNotSubscribed) {
  absl::Notification count;
  auto request = std::make_shared<Any>(any_request_1_);
  auto context = std::make_shared<AsyncContext<Any, Any>>(
      request, [&](AsyncContext<Any, Any>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        errors::SC_MESSAGE_ROUTER_REQUEST_NOT_SUBSCRIBED)));
        count.Notify();
      });
  queue_->TryEnqueue(context);

  count.WaitForNotification();
}

TEST_F(MessageRouterTest, SubscriptionConflict) {
  auto request_type = any_request_1_.type_url();
  EXPECT_SUCCESS(
      router_.Subscribe(request_type, [&](AsyncContext<Any, Any>& context) {}));

  EXPECT_THAT(
      router_.Subscribe(request_type, [&](AsyncContext<Any, Any>& context) {}),
      ResultIs(FailureExecutionResult(
          errors::SC_MESSAGE_ROUTER_REQUEST_ALREADY_SUBSCRIBED)));
}

TEST_F(MessageRouterTest, SingleMessage) {
  auto request = std::make_shared<Any>(any_request_1_);
  absl::Notification done;
  router_.Subscribe(any_request_1_.type_url(),
                    [&](AsyncContext<Any, Any>& context) {
                      TestStringResponse response;
                      response.set_response("test_response");
                      Any any_response;
                      any_response.PackFrom(response);
                      context.response = std::make_shared<Any>(any_response);
                      done.Notify();
                    });
  auto context = std::make_shared<AsyncContext<Any, Any>>(
      request, [&](AsyncContext<Any, Any>& context) {});
  queue_->TryEnqueue(context);

  ASSERT_TRUE(done.WaitForNotificationWithTimeout(absl::Seconds(5)));
  ASSERT_THAT(context->response, NotNull());
  TestStringResponse response;
  context->response->UnpackTo(&response);
  EXPECT_EQ(response.response(), "test_response");
}

TEST_F(MessageRouterTest, MultipleMessagesSingleSubscription) {
  absl::BlockingCounter counter(2);
  router_.Subscribe(any_request_1_.type_url(),
                    [&](AsyncContext<Any, Any>& context) {
                      TestStringResponse response;
                      response.set_response("test_response");
                      Any any_response;
                      any_response.PackFrom(response);
                      context.response = std::make_shared<Any>(any_response);
                      counter.DecrementCount();
                    });
  auto request_1 = std::make_shared<Any>(any_request_1_);
  auto context_1 = std::make_shared<AsyncContext<Any, Any>>(
      request_1, [&](AsyncContext<Any, Any>& context) {});
  auto request_2 = std::make_shared<Any>(any_request_1_);
  auto context_2 = std::make_shared<AsyncContext<Any, Any>>(
      request_2, [&](AsyncContext<Any, Any>& context) {});
  queue_->TryEnqueue(context_1);
  queue_->TryEnqueue(context_2);

  counter.Wait();
  ASSERT_THAT(context_1->response, NotNull());
  ASSERT_THAT(context_2->response, NotNull());
  TestStringResponse response_1;
  context_1->response->UnpackTo(&response_1);
  EXPECT_EQ(response_1.response(), "test_response");
  TestStringResponse response_2;
  context_2->response->UnpackTo(&response_2);
  EXPECT_EQ(response_2.response(), "test_response");
}

TEST_F(MessageRouterTest, MultipleSubscriptions) {
  absl::Mutex count_mu;
  int count_1 = 0;
  router_.Subscribe(any_request_1_.type_url(),
                    [&](AsyncContext<Any, Any>& context) {
                      absl::MutexLock lock(&count_mu);
                      count_1++;
                    });
  auto request_1 = std::make_shared<Any>(any_request_1_);
  auto context_1 = std::make_shared<AsyncContext<Any, Any>>(
      request_1, [&](AsyncContext<Any, Any>& context) {});

  int count_2 = 0;
  router_.Subscribe(any_request_2_.type_url(),
                    [&](AsyncContext<Any, Any>& context) {
                      absl::MutexLock lock(&count_mu);
                      count_2++;
                    });
  auto request_2 = std::make_shared<Any>(any_request_2_);
  auto context_2 = std::make_shared<AsyncContext<Any, Any>>(
      request_2, [&](AsyncContext<Any, Any>& context) {});

  queue_->TryEnqueue(context_1);
  queue_->TryEnqueue(context_2);

  {
    absl::MutexLock lock(&count_mu);
    auto condition_fn = [&] {
      count_mu.AssertReaderHeld();
      return count_1 == 1 && count_2 == 1;
    };
    count_mu.Await(absl::Condition(&condition_fn));
  }
}
}  // namespace google::scp::core::test
