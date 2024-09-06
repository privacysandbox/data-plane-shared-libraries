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

#include "src/cpio/client_providers/queue_client_provider/gcp/gcp_queue_client_provider.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>

#include <google/pubsub/v1/pubsub.grpc.pb.h>

#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/client_providers/interface/queue_client_provider_interface.h"
#include "src/cpio/client_providers/queue_client_provider/gcp/error_codes.h"
#include "src/cpio/client_providers/queue_client_provider/mock/gcp/mock_pubsub_stubs.h"
#include "src/cpio/common/gcp/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/queue_service/v1/queue_service.pb.h"

using google::cmrt::sdk::queue_service::v1::DeleteMessageRequest;
using google::cmrt::sdk::queue_service::v1::DeleteMessageResponse;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageRequest;
using google::cmrt::sdk::queue_service::v1::EnqueueMessageResponse;
using google::cmrt::sdk::queue_service::v1::GetTopMessageRequest;
using google::cmrt::sdk::queue_service::v1::GetTopMessageResponse;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutRequest;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutResponse;
using google::pubsub::v1::Publisher;
using google::pubsub::v1::Subscriber;
using google::scp::core::AsyncContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_ABORTED;
using google::scp::core::errors::SC_GCP_DATA_LOSS;
using google::scp::core::errors::SC_GCP_FAILED_PRECONDITION;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_PERMISSION_DENIED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_CONFIG_VISIBILITY_TIMEOUT;
using google::scp::core::errors::SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockPublisherStub;
using google::scp::cpio::client_providers::mock::MockSubscriberStub;
using grpc::Status;
using grpc::StatusCode;
using testing::_;
using testing::Eq;
using testing::NiceMock;
using testing::Return;
using testing::UnorderedElementsAre;

namespace google::scp::cpio::client_providers::gcp_queue_client::test {
namespace {
constexpr std::string_view kInstanceResourceName =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr std::string_view kQueueName = "queue_name";
constexpr std::string_view kMessageBody = "message_body";
constexpr std::string_view kMessageId = "message_id";
constexpr std::string_view kReceiptInfo = "receipt_info";
constexpr std::string_view kExpectedTopicName =
    "projects/123456789/topics/queue_name";
constexpr std::string_view kExpectedSubscriptionName =
    "projects/123456789/subscriptions/queue_name";
constexpr uint8_t kMaxNumberOfMessagesReceived = 1;
constexpr uint16_t kAckDeadlineSeconds = 60;
constexpr uint16_t kInvalidAckDeadlineSeconds = 1200;

class MockGcpPubSubStubFactory : public GcpPubSubStubFactory {
 public:
  MOCK_METHOD(std::shared_ptr<Publisher::StubInterface>, CreatePublisherStub,
              (std::string_view), (noexcept, override));
  MOCK_METHOD(std::shared_ptr<Subscriber::StubInterface>, CreateSubscriberStub,
              (std::string_view), (noexcept, override));
};

class GcpQueueClientProviderTest : public ::testing::Test {
 protected:
  GcpQueueClientProviderTest() {
    queue_client_options_.queue_name = kQueueName;
    mock_instance_client_provider_.instance_resource_name =
        kInstanceResourceName;

    mock_publisher_stub_ = std::make_shared<NiceMock<MockPublisherStub>>();
    mock_subscriber_stub_ = std::make_shared<NiceMock<MockSubscriberStub>>();

    mock_pubsub_stub_factory_ =
        std::make_shared<NiceMock<MockGcpPubSubStubFactory>>();
    ON_CALL(*mock_pubsub_stub_factory_, CreatePublisherStub)
        .WillByDefault(Return(mock_publisher_stub_));
    ON_CALL(*mock_pubsub_stub_factory_, CreateSubscriberStub)
        .WillByDefault(Return(mock_subscriber_stub_));

    queue_client_provider_.emplace(
        queue_client_options_, &mock_instance_client_provider_,
        &cpu_async_executor_, &io_async_executor_, mock_pubsub_stub_factory_);

    enqueue_message_context_.request =
        std::make_shared<EnqueueMessageRequest>();
    enqueue_message_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    get_top_message_context_.request = std::make_shared<GetTopMessageRequest>();
    get_top_message_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    update_message_visibility_timeout_context_.request =
        std::make_shared<UpdateMessageVisibilityTimeoutRequest>();
    update_message_visibility_timeout_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    delete_message_context_.request = std::make_shared<DeleteMessageRequest>();
    delete_message_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };
  }

  QueueClientOptions queue_client_options_;
  MockInstanceClientProvider mock_instance_client_provider_;
  MockAsyncExecutor cpu_async_executor_;
  MockAsyncExecutor io_async_executor_;
  std::shared_ptr<MockPublisherStub> mock_publisher_stub_;
  std::shared_ptr<MockSubscriberStub> mock_subscriber_stub_;
  std::shared_ptr<MockGcpPubSubStubFactory> mock_pubsub_stub_factory_;
  std::optional<GcpQueueClientProvider> queue_client_provider_;

  AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>
      enqueue_message_context_;

  AsyncContext<GetTopMessageRequest, GetTopMessageResponse>
      get_top_message_context_;

  AsyncContext<UpdateMessageVisibilityTimeoutRequest,
               UpdateMessageVisibilityTimeoutResponse>
      update_message_visibility_timeout_context_;

  AsyncContext<DeleteMessageRequest, DeleteMessageResponse>
      delete_message_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  absl::Mutex finish_called_mu_;
  bool finish_called_ ABSL_GUARDED_BY(finish_called_mu_) = false;
};

TEST_F(GcpQueueClientProviderTest, InitWithEmptyQueueName) {
  queue_client_options_.queue_name = "";
  MockAsyncExecutor cpu_async_executor;
  MockAsyncExecutor io_async_executor;
  GcpQueueClientProvider client(
      queue_client_options_, &mock_instance_client_provider_,
      &cpu_async_executor, &io_async_executor, mock_pubsub_stub_factory_);

  EXPECT_FALSE(client.Init().ok());
}

TEST_F(GcpQueueClientProviderTest, InitWithGetProjectIdFailure) {
  mock_instance_client_provider_.get_instance_resource_name_mock =
      absl::UnknownError("");
  MockAsyncExecutor cpu_async_executor;
  MockAsyncExecutor io_async_executor;
  GcpQueueClientProvider client(
      queue_client_options_, &mock_instance_client_provider_,
      &cpu_async_executor, &io_async_executor, mock_pubsub_stub_factory_);

  EXPECT_FALSE(client.Init().ok());
}

TEST_F(GcpQueueClientProviderTest, InitWithPublisherCreationFailure) {
  EXPECT_CALL(*mock_pubsub_stub_factory_, CreatePublisherStub)
      .WillOnce(Return(nullptr));

  EXPECT_FALSE(queue_client_provider_->Init().ok());
}

TEST_F(GcpQueueClientProviderTest, InitWithSubscriberCreationFailure) {
  EXPECT_CALL(*mock_pubsub_stub_factory_, CreateSubscriberStub)
      .WillOnce(Return(nullptr));

  EXPECT_FALSE(queue_client_provider_->Init().ok());
}

MATCHER_P(MessageHasBody, message_body, "") {
  return ExplainMatchResult(Eq(message_body), arg.data(), result_listener);
}

MATCHER_P2(HasPublishParams, topic_name, message_body, "") {
  return ExplainMatchResult(Eq(topic_name), arg.topic(), result_listener) &&
         ExplainMatchResult(UnorderedElementsAre(MessageHasBody(message_body)),
                            arg.messages(), result_listener);
}

TEST_F(GcpQueueClientProviderTest, EnqueueMessageSuccess) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_publisher_stub_,
              Publish(_, HasPublishParams(kExpectedTopicName, kMessageBody), _))
      .WillOnce([](auto, auto, auto* publish_response) {
        publish_response->add_message_ids(kMessageId);
        return Status(StatusCode::OK, "");
      });
  enqueue_message_context_.request->set_message_body(kMessageBody);
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_SUCCESS(enqueue_message_context.result);

        EXPECT_EQ(enqueue_message_context.response->message_id(), kMessageId);
        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest, EnqueueMessageFailureWithEmptyMessageBody) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  enqueue_message_context_.request->set_message_body("");
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_THAT(enqueue_message_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_FALSE(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest, EnqueueMessageFailureWithPubSubError) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_publisher_stub_,
              Publish(_, HasPublishParams(kExpectedTopicName, kMessageBody), _))
      .WillOnce(Return(Status(StatusCode::PERMISSION_DENIED, "")));

  enqueue_message_context_.request->set_message_body(kMessageBody);
  enqueue_message_context_.callback =
      [this](AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
                 enqueue_message_context) {
        EXPECT_THAT(enqueue_message_context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_PERMISSION_DENIED)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->EnqueueMessage(enqueue_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

MATCHER_P2(HasPullParams, subscription_name, max_messages, "") {
  return ExplainMatchResult(Eq(subscription_name), arg.subscription(),
                            result_listener) &&
         ExplainMatchResult(Eq(max_messages), arg.max_messages(),
                            result_listener);
}

TEST_F(GcpQueueClientProviderTest, GetTopMessageSuccess) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce([](auto, auto, auto* pull_response) {
        auto* received_message = pull_response->add_received_messages();
        received_message->mutable_message()->set_data(kMessageBody);
        received_message->mutable_message()->set_message_id(kMessageId);
        received_message->set_ack_id(kReceiptInfo);
        return Status(StatusCode::OK, "");
      });

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_SUCCESS(get_top_message_context.result);

        EXPECT_EQ(get_top_message_context.response->message_id(), kMessageId);
        EXPECT_EQ(get_top_message_context.response->message_body(),
                  kMessageBody);
        EXPECT_EQ(get_top_message_context.response->receipt_info(),
                  kReceiptInfo);

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->GetTopMessage(get_top_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest, GetTopMessageWithNoMessagesReturns) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce([](auto, auto, auto* pull_response) {
        return Status(StatusCode::OK, "");
      });

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_SUCCESS(get_top_message_context.result);

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->GetTopMessage(get_top_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest, GetTopMessageFailureWithPubSubError) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce(Return(Status(StatusCode::ABORTED, "")));

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_THAT(get_top_message_context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_ABORTED)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->GetTopMessage(get_top_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest,
       GetTopMessageFailureWithNumberOfMessagesReceivedExceedingLimit) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_subscriber_stub_,
              Pull(_,
                   HasPullParams(kExpectedSubscriptionName,
                                 kMaxNumberOfMessagesReceived),
                   _))
      .WillOnce([](auto, auto, auto* pull_response) {
        pull_response->add_received_messages();
        pull_response->add_received_messages();
        return Status(StatusCode::OK, "");
      });

  get_top_message_context_.callback =
      [this](AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
                 get_top_message_context) {
        EXPECT_THAT(
            get_top_message_context.result,
            ResultIs(FailureExecutionResult(
                SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->GetTopMessage(get_top_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

MATCHER_P3(HasModifyAckDeadlineParams, subscription_name, ack_id,
           ack_deadline_seconds, "") {
  return ExplainMatchResult(Eq(subscription_name), arg.subscription(),
                            result_listener) &&
         ExplainMatchResult(UnorderedElementsAre(Eq(ack_id)), arg.ack_ids(),
                            result_listener) &&
         ExplainMatchResult(Eq(ack_deadline_seconds),
                            arg.ack_deadline_seconds(), result_listener);
}

TEST_F(GcpQueueClientProviderTest, UpdateMessageVisibilityTimeoutSuccess) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_subscriber_stub_,
              ModifyAckDeadline(
                  _,
                  HasModifyAckDeadlineParams(kExpectedSubscriptionName,
                                             kReceiptInfo, kAckDeadlineSeconds),
                  _))
      .WillOnce(Return(Status(StatusCode::OK, "")));

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kAckDeadlineSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_SUCCESS(update_message_visibility_timeout_context.result);

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(queue_client_provider_
                  ->UpdateMessageVisibilityTimeout(
                      update_message_visibility_timeout_context_)
                  .ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutFailureWithEmptyReceiptInfo) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  update_message_visibility_timeout_context_.request->set_receipt_info("");
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(update_message_visibility_timeout_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_FALSE(queue_client_provider_
                   ->UpdateMessageVisibilityTimeout(
                       update_message_visibility_timeout_context_)
                   .ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutFailureWithInvalidMessageLifetime) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kInvalidAckDeadlineSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(
            update_message_visibility_timeout_context.result,
            ResultIs(FailureExecutionResult(
                SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_FALSE(queue_client_provider_
                   ->UpdateMessageVisibilityTimeout(
                       update_message_visibility_timeout_context_)
                   .ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutFailureWithPubSubError) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(*mock_subscriber_stub_,
              ModifyAckDeadline(
                  _,
                  HasModifyAckDeadlineParams(kExpectedSubscriptionName,
                                             kReceiptInfo, kAckDeadlineSeconds),
                  _))
      .WillOnce(Return(Status(StatusCode::FAILED_PRECONDITION, "")));

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kAckDeadlineSeconds);
  update_message_visibility_timeout_context_.callback =
      [this](AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                          UpdateMessageVisibilityTimeoutResponse>&
                 update_message_visibility_timeout_context) {
        EXPECT_THAT(
            update_message_visibility_timeout_context.result,
            ResultIs(FailureExecutionResult(SC_GCP_FAILED_PRECONDITION)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(queue_client_provider_
                  ->UpdateMessageVisibilityTimeout(
                      update_message_visibility_timeout_context_)
                  .ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

MATCHER_P2(HasAcknowledgeParams, subscription_name, ack_id, "") {
  return ExplainMatchResult(Eq(subscription_name), arg.subscription(),
                            result_listener) &&
         ExplainMatchResult(UnorderedElementsAre(Eq(ack_id)), arg.ack_ids(),
                            result_listener);
}

TEST_F(GcpQueueClientProviderTest, DeleteMessageSuccess) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(
      *mock_subscriber_stub_,
      Acknowledge(
          _, HasAcknowledgeParams(kExpectedSubscriptionName, kReceiptInfo), _))
      .WillOnce(Return(Status(StatusCode::OK, "")));

  delete_message_context_.request->set_receipt_info(kReceiptInfo);
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_SUCCESS(delete_message_context.result);

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->DeleteMessage(delete_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest, DeleteMessageFailureWithEmptyReceiptInfo) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  delete_message_context_.request->set_receipt_info("");
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_THAT(delete_message_context.result,
                    ResultIs(FailureExecutionResult(
                        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_FALSE(
      queue_client_provider_->DeleteMessage(delete_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpQueueClientProviderTest, DeleteMessageFailureWithPubSubError) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  EXPECT_CALL(
      *mock_subscriber_stub_,
      Acknowledge(
          _, HasAcknowledgeParams(kExpectedSubscriptionName, kReceiptInfo), _))
      .WillOnce(Return(Status(StatusCode::DATA_LOSS, "")));

  delete_message_context_.request->set_receipt_info(kReceiptInfo);
  delete_message_context_.callback =
      [this](AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
                 delete_message_context) {
        EXPECT_THAT(delete_message_context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_DATA_LOSS)));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_TRUE(
      queue_client_provider_->DeleteMessage(delete_message_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}
}  // namespace
}  // namespace google::scp::cpio::client_providers::gcp_queue_client::test
