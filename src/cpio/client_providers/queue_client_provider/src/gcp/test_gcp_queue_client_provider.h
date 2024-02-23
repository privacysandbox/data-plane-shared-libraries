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

#ifndef CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_SRC_GCP_TEST_GCP_QUEUE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_SRC_GCP_TEST_GCP_QUEUE_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include "src/cpio/client_providers/queue_client_provider/src/gcp/gcp_queue_client_provider.h"

namespace google::scp::cpio::client_providers {
/// QueueClientOptions for testing on GCP.
struct TestGcpQueueClientOptions : public QueueClientOptions {
  virtual ~TestGcpQueueClientOptions() = default;

  TestGcpQueueClientOptions() = default;

  explicit TestGcpQueueClientOptions(QueueClientOptions options)
      : QueueClientOptions(std::move(options)) {}

  // TODO: get rid of shared_ptr.
  std::shared_ptr<std::string> pubsub_client_endpoint_override =
      std::make_shared<std::string>();

  std::string access_token;
};

/*! @copydoc GcpPubSubStubFactory
 */
class TestGcpPubSubStubFactory : public GcpPubSubStubFactory {
 private:
  std::shared_ptr<grpc::Channel> GetPubSubChannel(
      const QueueClientOptions& options) noexcept override;
};

/*! @copydoc GcpQueueClientProvider
 */
class TestGcpQueueClientProvider : public GcpQueueClientProvider {
 public:
  explicit TestGcpQueueClientProvider(
      TestGcpQueueClientOptions queue_client_options,
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* cpu_async_executor,
      core::AsyncExecutorInterface* io_async_executor)
      : GcpQueueClientProvider(std::move(queue_client_options),
                               instance_client_provider, cpu_async_executor,
                               io_async_executor,
                               std::make_unique<TestGcpPubSubStubFactory>()) {}
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_QUEUE_CLIENT_PROVIDER_SRC_GCP_TEST_GCP_QUEUE_CLIENT_PROVIDER_H_
