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

#ifndef CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_GCP_GCP_QUEUE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_GCP_GCP_QUEUE_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include <google/pubsub/v1/pubsub.grpc.pb.h>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/cpio/client_providers/interface/queue_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
inline constexpr std::string_view kPubSubEndpointUri = "pubsub.googleapis.com";

/// Provides GCP Pub/Sub stubs.
class GcpPubSubStubFactory {
 public:
  /**
   * @brief Creates Publisher Stub.
   *
   * @param options the QueueClientOptions.
   * @return std::shared_ptr<google::pubsub::v1::Publisher::Stub> the creation
   * result.
   */
  virtual std::shared_ptr<google::pubsub::v1::Publisher::StubInterface>
  CreatePublisherStub(std::string_view queue_name) noexcept;
  /**
   * @brief Creates Subscriber Stub.
   *
   * @param options the QueueClientOptions.
   * @return std::shared_ptr<google::pubsub::v1::Subscriber::Stub> the creation
   * result.
   */
  virtual std::shared_ptr<google::pubsub::v1::Subscriber::StubInterface>
  CreateSubscriberStub(std::string_view queue_name) noexcept;

  virtual ~GcpPubSubStubFactory() = default;

 private:
  /**
   * @brief Gets Pub/Sub Channel.
   *
   * @param queue_name
   * @return std::shared_ptr<grpc::Channel> the creation result.
   */
  virtual std::shared_ptr<grpc::Channel> GetPubSubChannel(
      std::string_view queue_name) noexcept;

 protected:
  // An Instance of the gRPC Channel for Publisher and Subscriber Stubs.
  std::shared_ptr<grpc::Channel> channel_;
};

/*! @copydoc QueueClientProviderInterface
 */
class GcpQueueClientProvider : public QueueClientProviderInterface {
 public:
  virtual ~GcpQueueClientProvider() = default;

  explicit GcpQueueClientProvider(
      QueueClientOptions queue_client_options,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client_provider,
      absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor,
      absl::Nonnull<std::shared_ptr<GcpPubSubStubFactory>> pubsub_stub_factory =
          std::make_shared<GcpPubSubStubFactory>())
      : queue_name_(std::move(queue_client_options.queue_name)),
        project_id_(std::move(queue_client_options.project_id)),
        instance_client_provider_(instance_client_provider),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        pubsub_stub_factory_(std::move(pubsub_stub_factory)) {}

  absl::Status Init() noexcept;

  absl::Status EnqueueMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept override;

  absl::Status GetTopMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept override;

  absl::Status UpdateMessageVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept override;

  absl::Status DeleteMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept override;

 private:
  /**
   * @brief Is called when the object is returned from the GCP Publish callback.
   *
   * @param enqueue_message_context the enqueue message context.
   */
  void EnqueueMessageAsync(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept;

  /**
   * @brief Is called when the object is returned from the GCP Pull callback.
   *
   * @param get_top_message_context the get top message context.
   */
  void GetTopMessageAsync(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept;

  /**
   * @brief Is called when the object is returned from the GCP Update Ack
   * Deadline callback.
   *
   * @param update_message_visibility_timeout_context the update message
   * visibility timeout context.
   */
  void UpdateMessageVisibilityTimeoutAsync(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept;

  /**
   * @brief Is called when the object is returned from the GCP Acknowledge
   * callback.
   *
   * @param delete_message_context the delete message context.
   */
  void DeleteMessageAsync(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept;

  /// The configuration for queue client.
  std::string queue_name_;

  /// Project ID of current instance.
  std::string project_id_;

  /// The instance client provider.
  InstanceClientProviderInterface* instance_client_provider_;

  /// The instance of the async executor.
  core::AsyncExecutorInterface* cpu_async_executor_;
  core::AsyncExecutorInterface* io_async_executor_;

  /// Topic name of current instance. Format is
  /// projects/{project_id}/topics/{topic_name}.
  std::string topic_name_;

  /// Subscription name of current instance.Format is
  /// projects/{project_id}/subscriptions/{subscription_name}.
  std::string subscription_name_;

  // TODO(b/321321138): Fix test to make pointer unnecessary.
  /// An Instance of the GCP Pub/Sub stub factory.
  std::shared_ptr<GcpPubSubStubFactory> pubsub_stub_factory_;

  /// An Instance of the GCP Publisher stub.
  std::shared_ptr<google::pubsub::v1::Publisher::StubInterface> publisher_stub_;

  /// An Instance of the GCP Subscriber stub.
  std::shared_ptr<google::pubsub::v1::Subscriber::StubInterface>
      subscriber_stub_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_GCP_GCP_QUEUE_CLIENT_PROVIDER_H_
