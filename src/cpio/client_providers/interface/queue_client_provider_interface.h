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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_QUEUE_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_QUEUE_CLIENT_PROVIDER_INTERFACE_H_

#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/service_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/queue_service/v1/queue_service.pb.h"

#include "instance_client_provider_interface.h"

namespace google::scp::cpio::client_providers {

/**
 * @brief Interface responsible for queuing messages.
 */
class QueueClientProviderInterface {
 public:
  virtual ~QueueClientProviderInterface() = default;
  /**
   * @brief Enqueue a message to the queue.
   * @param enqueue_message_context context of the operation.
   * @return absl::Status status of the operation.
   */
  virtual absl::Status EnqueueMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                         cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&
          enqueue_message_context) noexcept = 0;
  /**
   * @brief Get top message from the queue.
   * @param get_top_message_context context of the operation.
   * @return absl::Status status of the operation.
   */
  virtual absl::Status GetTopMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                         cmrt::sdk::queue_service::v1::GetTopMessageResponse>&
          get_top_message_context) noexcept = 0;
  /**
   * @brief Update visibility timeout of a message from the queue.
   * @param update_message_visibility_timeout_context context of the operation.
   * @return absl::Status status of the operation.
   */
  virtual absl::Status UpdateMessageVisibilityTimeout(
      core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutResponse>&
          update_message_visibility_timeout_context) noexcept = 0;
  /**
   * @brief Delete a message from the queue.
   * @param delete_message_context context of the operation.
   * @return absl::Status status of the operation.
   */
  virtual absl::Status DeleteMessage(
      core::AsyncContext<cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                         cmrt::sdk::queue_service::v1::DeleteMessageResponse>&
          delete_message_context) noexcept = 0;
};

/// Configurations for QueueClient.
struct QueueClientOptions {
  /**
   * @brief Required. The identifier of the queue. The queue is per client per
   * service. In AWS SQS, it's the queue name. In GCP Pub/Sub, there is only one
   * Subscription subscribes to the Topic, so the queue name is tied to Topic Id
   * and Subscription Id.
   *
   */
  std::string queue_name;
  std::string project_id;
};

class QueueClientProviderFactory {
 public:
  /**
   * @brief Factory to create QueueClientProvider.
   *
   * @param options QueueClientOptions.
   * @param instance_client Instance Client.
   * @param cpu_async_executor CPU Async Eexcutor.
   * @param io_async_executor IO Async Eexcutor.
   * @return std::unique_ptr<QueueClientProviderInterface> created
   * QueueClientProviderProvider.
   */
  static absl::StatusOr<std::unique_ptr<QueueClientProviderInterface>> Create(
      QueueClientOptions options,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client,
      absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_QUEUE_CLIENT_PROVIDER_INTERFACE_H_
