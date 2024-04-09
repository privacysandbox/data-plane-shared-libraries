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

#include "gcp_queue_client_provider.h"

#include <string>

#include <grpcpp/grpcpp.h>

#include "absl/base/nullability.h"
#include "absl/strings/substitute.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/instance_client_provider/gcp/gcp_instance_client_utils.h"
#include "src/cpio/common/gcp/gcp_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/queue_service/v1/queue_service.pb.h"
#include "src/util/status_macro/status_macros.h"

#include "error_codes.h"

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
using google::protobuf::Empty;
using google::pubsub::v1::AcknowledgeRequest;
using google::pubsub::v1::ModifyAckDeadlineRequest;
using google::pubsub::v1::Publisher;
using google::pubsub::v1::PublishRequest;
using google::pubsub::v1::PublishResponse;
using google::pubsub::v1::PubsubMessage;
using google::pubsub::v1::PullRequest;
using google::pubsub::v1::PullResponse;
using google::pubsub::v1::Subscriber;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::AsyncPriority;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_CONFIG_VISIBILITY_TIMEOUT;
using google::scp::core::errors::SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_MISMATCH;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::core::errors::
    SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED;
using google::scp::cpio::client_providers::GcpInstanceClientUtils;
using google::scp::cpio::common::GcpUtils;
using grpc::Channel;
using grpc::ChannelArguments;
using grpc::ClientContext;
using grpc::CreateChannel;
using grpc::CreateCustomChannel;
using grpc::GoogleDefaultCredentials;
using grpc::StatusCode;
using grpc::StubOptions;

namespace {
constexpr std::string_view kGcpQueueClientProvider = "GcpQueueClientProvider";
constexpr std::string_view kGcpTopicFormatString = "projects/$0/topics/$1";
constexpr std::string_view kGcpSubscriptionFormatString =
    "projects/$0/subscriptions/$1";
constexpr uint8_t kMaxNumberOfMessagesReceived = 1;
constexpr uint16_t kMaxAckDeadlineSeconds = 600;
}  // namespace

namespace google::scp::cpio::client_providers {

absl::Status GcpQueueClientProvider::Init() noexcept {
  if (queue_name_.empty()) {
    const ExecutionResult execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Invalid queue name.");
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (project_id_.empty()) {
    auto project_id_or =
        GcpInstanceClientUtils::GetCurrentProjectId(*instance_client_provider_);
    if (!project_id_or.Successful()) {
      SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, project_id_or.result(),
                "Failed to get project ID for current instance");
      return absl::InvalidArgumentError(
          google::scp::core::errors::GetErrorMessage(
              project_id_or.result().status_code));
    }
    project_id_ = std::move(*project_id_or);
  }

  publisher_stub_ = pubsub_stub_factory_->CreatePublisherStub(queue_name_);
  if (!publisher_stub_) {
    const ExecutionResult execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_PUBLISHER_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Failed to create publisher.");
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  subscriber_stub_ = pubsub_stub_factory_->CreateSubscriberStub(queue_name_);
  if (!subscriber_stub_) {
    const ExecutionResult execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_SUBSCRIBER_REQUIRED);
    SCP_ERROR(kGcpQueueClientProvider, kZeroUuid, execution_result,
              "Failed to create subscriber.");
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  topic_name_ =
      absl::Substitute(kGcpTopicFormatString, project_id_, queue_name_);
  subscription_name_ =
      absl::Substitute(kGcpSubscriptionFormatString, project_id_, queue_name_);

  return absl::OkStatus();
}

absl::Status GcpQueueClientProvider::EnqueueMessage(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  if (enqueue_message_context.request->message_body().empty()) {
    const ExecutionResult execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context, execution_result,
        "Failed to enqueue message due to missing message body in "
        "the request for topic: %s",
        topic_name_.c_str());
    enqueue_message_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (const ExecutionResult execution_result = io_async_executor_->Schedule(
          [this, enqueue_message_context]() mutable {
            EnqueueMessageAsync(enqueue_message_context);
          },
          AsyncPriority::Normal);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context,
        enqueue_message_context.result,
        "Enqueue Message request failed to be scheduled. Topic: %s",
        topic_name_.c_str());
    enqueue_message_context.Finish(execution_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }
  return absl::OkStatus();
}

void GcpQueueClientProvider::EnqueueMessageAsync(
    AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
        enqueue_message_context) noexcept {
  PublishRequest publish_request;
  publish_request.set_topic(topic_name_);
  PubsubMessage* message = publish_request.add_messages();
  message->set_data(enqueue_message_context.request->message_body().c_str());

  ClientContext client_context;
  PublishResponse publish_response;
  auto status = publisher_stub_->Publish(&client_context, publish_request,
                                         &publish_response);
  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context, execution_result,
        "Failed to enqueue message due to GCP Pub/Sub service error. Topic: %s",
        topic_name_.c_str());
    FinishContext(execution_result, enqueue_message_context,
                  *cpu_async_executor_);
    return;
  }

  const auto& message_ids = publish_response.message_ids();
  // This should never happen.
  if (message_ids.size() != 1) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_MISMATCH);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, enqueue_message_context, execution_result,
        "The number of message ids received from the response does "
        "not match the number of message in the request. Topic: %s",
        topic_name_.c_str());
    FinishContext(execution_result, enqueue_message_context,
                  *cpu_async_executor_);
    return;
  }

  auto response = std::make_shared<EnqueueMessageResponse>();
  response->set_message_id(publish_response.message_ids(0));
  enqueue_message_context.response = std::move(response);
  FinishContext(SuccessExecutionResult(), enqueue_message_context,
                *cpu_async_executor_);
}

absl::Status GcpQueueClientProvider::GetTopMessage(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  if (const ExecutionResult execution_result = io_async_executor_->Schedule(
          [this, get_top_message_context]() mutable {
            GetTopMessageAsync(get_top_message_context);
          },
          AsyncPriority::Normal);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, get_top_message_context,
        get_top_message_context.result,
        "Get Top Message request failed to be scheduled. Topic: %s",
        topic_name_.c_str());
    get_top_message_context.Finish(execution_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }
  return absl::OkStatus();
}

void GcpQueueClientProvider::GetTopMessageAsync(
    AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
        get_top_message_context) noexcept {
  PullRequest pull_request;
  pull_request.set_subscription(subscription_name_);
  pull_request.set_max_messages(kMaxNumberOfMessagesReceived);
  ClientContext client_context;
  PullResponse pull_response;
  auto status =
      subscriber_stub_->Pull(&client_context, pull_request, &pull_response);

  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, get_top_message_context, execution_result,
        "Failed to get top message due to GCP Pub/Sub service error. "
        "Subscription: %s",
        subscription_name_.c_str());
    FinishContext(execution_result, get_top_message_context,
                  *cpu_async_executor_);
    return;
  }

  const auto& received_messages = pull_response.received_messages();

  // This should never happen.
  if (received_messages.size() > kMaxNumberOfMessagesReceived) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, get_top_message_context, execution_result,
        "The number of messages received from the response is larger "
        "than the maximum number. Subscription: %s",
        subscription_name_.c_str());
    FinishContext(execution_result, get_top_message_context,
                  *cpu_async_executor_);
    return;
  }

  if (received_messages.empty()) {
    get_top_message_context.response =
        std::make_shared<GetTopMessageResponse>();
    FinishContext(SuccessExecutionResult(), get_top_message_context,
                  *cpu_async_executor_);
    return;
  }

  auto received_message = received_messages.at(0);
  auto response = std::make_shared<GetTopMessageResponse>();
  response->set_message_body(received_message.mutable_message()->data());
  response->set_message_id(received_message.mutable_message()->message_id());
  response->set_receipt_info(received_message.ack_id());
  get_top_message_context.response = std::move(response);

  FinishContext(SuccessExecutionResult(), get_top_message_context,
                *cpu_async_executor_);
}

absl::Status GcpQueueClientProvider::UpdateMessageVisibilityTimeout(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  if (update_message_visibility_timeout_context.request->receipt_info()
          .empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to update message visibility timeout due to missing "
        "receipt info in the request. Subscription: %s",
        subscription_name_.c_str());
    update_message_visibility_timeout_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  auto lifetime_in_seconds = update_message_visibility_timeout_context.request
                                 ->message_visibility_timeout()
                                 .seconds();
  if (lifetime_in_seconds < 0 || lifetime_in_seconds > kMaxAckDeadlineSeconds) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to update message visibility timeout due to invalid "
        "message visibility timeout in the request. Subscription: %s",
        subscription_name_.c_str());
    update_message_visibility_timeout_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (const ExecutionResult execution_result = io_async_executor_->Schedule(
          [this, update_message_visibility_timeout_context]() mutable {
            UpdateMessageVisibilityTimeoutAsync(
                update_message_visibility_timeout_context);
          },
          AsyncPriority::Normal);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpQueueClientProvider,
                      update_message_visibility_timeout_context,
                      update_message_visibility_timeout_context.result,
                      "Update message visibility timeout request failed to be "
                      "scheduled for subscription: %s",
                      subscription_name_.c_str());
    update_message_visibility_timeout_context.Finish(execution_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }
  return absl::OkStatus();
}

void GcpQueueClientProvider::UpdateMessageVisibilityTimeoutAsync(
    AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                 UpdateMessageVisibilityTimeoutResponse>&
        update_message_visibility_timeout_context) noexcept {
  ModifyAckDeadlineRequest modify_ack_deadline_request;
  modify_ack_deadline_request.set_subscription(subscription_name_);
  modify_ack_deadline_request.add_ack_ids(
      update_message_visibility_timeout_context.request->receipt_info()
          .c_str());
  modify_ack_deadline_request.set_ack_deadline_seconds(
      update_message_visibility_timeout_context.request
          ->message_visibility_timeout()
          .seconds());

  ClientContext client_context;
  Empty modify_ack_deadline_response;
  auto status = subscriber_stub_->ModifyAckDeadline(
      &client_context, modify_ack_deadline_request,
      &modify_ack_deadline_response);
  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, update_message_visibility_timeout_context,
        execution_result,
        "Failed to modify message ack deadline due to GCP Pub/Sub "
        "service error. Subscription: %s",
        subscription_name_.c_str());
    FinishContext(execution_result, update_message_visibility_timeout_context,
                  *cpu_async_executor_);
    return;
  }

  update_message_visibility_timeout_context.response =
      std::make_shared<UpdateMessageVisibilityTimeoutResponse>();
  FinishContext(SuccessExecutionResult(),
                update_message_visibility_timeout_context,
                *cpu_async_executor_);
}

absl::Status GcpQueueClientProvider::DeleteMessage(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context) noexcept {
  if (delete_message_context.request->receipt_info().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_QUEUE_CLIENT_PROVIDER_INVALID_MESSAGE);
    SCP_ERROR_CONTEXT(kGcpQueueClientProvider, delete_message_context,
                      execution_result,
                      "Failed to delete message due to missing receipt info in "
                      "the request. Subscription: %s",
                      subscription_name_.c_str());
    delete_message_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (const ExecutionResult execution_result = io_async_executor_->Schedule(
          [this, delete_message_context]() mutable {
            DeleteMessageAsync(delete_message_context);
          },
          AsyncPriority::Normal);
      !execution_result.Successful()) {
    SCP_ERROR_CONTEXT(
        kGcpQueueClientProvider, delete_message_context,
        delete_message_context.result,
        "Delete request failed to be scheduled for subscription: %s",
        subscription_name_.c_str());
    delete_message_context.Finish(execution_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }
  return absl::OkStatus();
}

void GcpQueueClientProvider::DeleteMessageAsync(
    AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
        delete_message_context) noexcept {
  AcknowledgeRequest acknowledge_request;
  acknowledge_request.set_subscription(subscription_name_);
  acknowledge_request.add_ack_ids(
      delete_message_context.request->receipt_info().c_str());

  ClientContext client_context;
  Empty acknowledge_response;
  auto status = subscriber_stub_->Acknowledge(
      &client_context, acknowledge_request, &acknowledge_response);
  if (!status.ok()) {
    auto execution_result = GcpUtils::GcpErrorConverter(status);
    SCP_ERROR_CONTEXT(kGcpQueueClientProvider, delete_message_context,
                      execution_result,
                      "Failed to acknowledge message due to GCP Pub/Sub "
                      "service error. Subscription: %s",
                      subscription_name_.c_str());
    FinishContext(execution_result, delete_message_context,
                  *cpu_async_executor_);
    return;
  }

  delete_message_context.response = std::make_shared<DeleteMessageResponse>();
  FinishContext(SuccessExecutionResult(), delete_message_context,
                *cpu_async_executor_);
}

std::shared_ptr<Channel> GcpPubSubStubFactory::GetPubSubChannel(
    std::string_view /*queue_name*/) noexcept {
  if (!channel_) {
    ChannelArguments args;
    args.SetInt(GRPC_ARG_ENABLE_RETRIES, 1);  // enable
    channel_ = CreateCustomChannel(std::string(kPubSubEndpointUri),
                                   GoogleDefaultCredentials(), args);
  }
  return channel_;
}

std::shared_ptr<Publisher::StubInterface>
GcpPubSubStubFactory::CreatePublisherStub(
    std::string_view queue_name) noexcept {
  return std::unique_ptr<Publisher::Stub>(
      Publisher::NewStub(GetPubSubChannel(queue_name), StubOptions()));
}

std::shared_ptr<Subscriber::StubInterface>
GcpPubSubStubFactory::CreateSubscriberStub(
    std::string_view queue_name) noexcept {
  return std::unique_ptr<Subscriber::Stub>(
      Subscriber::NewStub(GetPubSubChannel(queue_name), StubOptions()));
}

absl::StatusOr<std::unique_ptr<QueueClientProviderInterface>>
QueueClientProviderFactory::Create(
    QueueClientOptions options,
    absl::Nonnull<InstanceClientProviderInterface*> instance_client,
    absl::Nonnull<AsyncExecutorInterface*> cpu_async_executor,
    absl::Nonnull<AsyncExecutorInterface*> io_async_executor) noexcept {
  auto provider = std::make_unique<GcpQueueClientProvider>(
      std::move(options), instance_client, cpu_async_executor,
      io_async_executor);
  PS_RETURN_IF_ERROR(provider->Init());
  return provider;
}
}  // namespace google::scp::cpio::client_providers
