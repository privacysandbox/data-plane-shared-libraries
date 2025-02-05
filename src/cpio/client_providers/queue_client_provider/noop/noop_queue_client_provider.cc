/*
 * Copyright 2025 Google LLC
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

#include <memory>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/queue_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

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
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::cpio::client_providers::QueueClientProviderInterface;

namespace {
class NoopQueueClientProvider : public QueueClientProviderInterface {
 public:
  absl::Status EnqueueMessage(
      AsyncContext<EnqueueMessageRequest, EnqueueMessageResponse>&
      /* enqueue_message_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status GetTopMessage(
      AsyncContext<GetTopMessageRequest, GetTopMessageResponse>&
      /* get_top_message_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status UpdateMessageVisibilityTimeout(
      AsyncContext<UpdateMessageVisibilityTimeoutRequest,
                   UpdateMessageVisibilityTimeoutResponse>&
      /* update_message_visibility_timeout_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status DeleteMessage(
      AsyncContext<DeleteMessageRequest, DeleteMessageResponse>&
      /* delete_message_context */) noexcept override {
    return absl::UnimplementedError("");
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::StatusOr<std::unique_ptr<QueueClientProviderInterface>>
QueueClientProviderFactory::Create(
    QueueClientOptions /* options */,
    absl::Nonnull<InstanceClientProviderInterface*> /* instance_client */,
    absl::Nonnull<core::AsyncExecutorInterface*> /* cpu_async_executor */,
    absl::Nonnull<
        core::AsyncExecutorInterface*> /* io_async_executor */) noexcept {
  return std::make_unique<NoopQueueClientProvider>();
}
}  // namespace google::scp::cpio::client_providers
