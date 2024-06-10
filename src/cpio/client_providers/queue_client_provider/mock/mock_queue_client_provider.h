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

#ifndef CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_MOCK_MOCK_QUEUE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_MOCK_MOCK_QUEUE_CLIENT_PROVIDER_H_

#include <gmock/gmock.h>

#include "src/cpio/client_providers/interface/queue_client_provider_interface.h"

namespace google::scp::cpio::client_providers::mock {

/*! @copydoc QueueClientProviderInterface
 */
class MockQueueClientProvider : public QueueClientProviderInterface {
 public:
  MOCK_METHOD(absl::Status, EnqueueMessage,
              ((core::AsyncContext<
                  cmrt::sdk::queue_service::v1::EnqueueMessageRequest,
                  cmrt::sdk::queue_service::v1::EnqueueMessageResponse>&)),
              (noexcept, override));

  MOCK_METHOD(absl::Status, GetTopMessage,
              ((core::AsyncContext<
                  cmrt::sdk::queue_service::v1::GetTopMessageRequest,
                  cmrt::sdk::queue_service::v1::GetTopMessageResponse>&)),
              (noexcept, override));

  MOCK_METHOD(
      absl::Status, UpdateMessageVisibilityTimeout,
      ((core::AsyncContext<
          cmrt::sdk::queue_service::v1::UpdateMessageVisibilityTimeoutRequest,
          cmrt::sdk::queue_service::v1::
              UpdateMessageVisibilityTimeoutResponse>&)),
      (noexcept, override));

  MOCK_METHOD(absl::Status, DeleteMessage,
              ((core::AsyncContext<
                  cmrt::sdk::queue_service::v1::DeleteMessageRequest,
                  cmrt::sdk::queue_service::v1::DeleteMessageResponse>&)),
              (noexcept, override));
};

}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_MOCK_MOCK_QUEUE_CLIENT_PROVIDER_H_
