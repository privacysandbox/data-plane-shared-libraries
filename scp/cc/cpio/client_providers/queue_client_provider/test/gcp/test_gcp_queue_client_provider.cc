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

#include "test_gcp_queue_client_provider.h"

#include <memory>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>
#include <grpcpp/security/credentials.h>

#include "cpio/client_providers/interface/queue_client_provider_interface.h"

using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using grpc::Channel;
using grpc::ChannelArguments;
using grpc::CreateCustomChannel;

namespace google::scp::cpio::client_providers {
std::shared_ptr<Channel> TestGcpPubSubStubFactory::GetPubSubChannel(
    const QueueClientOptions& options) noexcept {
  if (!channel_) {
    ChannelArguments args;
    args.SetInt(GRPC_ARG_ENABLE_RETRIES, 1);  // enable
    const auto& test_options =
        dynamic_cast<const TestGcpQueueClientOptions&>(options);
    if (test_options.access_token.empty()) {
      channel_ =
          CreateCustomChannel(*test_options.pubsub_client_endpoint_override,
                              grpc::InsecureChannelCredentials(), args);
    } else {
      auto call_credentials =
          grpc::AccessTokenCredentials(test_options.access_token);
      grpc::SslCredentialsOptions ssl_options;
      auto ssl_credentials = grpc::SslCredentials(ssl_options);

      channel_ = CreateCustomChannel(
          kPubSubEndpointUri,
          grpc::CompositeChannelCredentials(ssl_credentials, call_credentials),
          args);
    }
  }
  return channel_;
}

std::unique_ptr<QueueClientProviderInterface>
QueueClientProviderFactory::Create(
    QueueClientOptions options,
    InstanceClientProviderInterface* instance_client_provider,
    AsyncExecutorInterface* cpu_async_executor,
    AsyncExecutorInterface* io_async_executor) noexcept {
  return std::make_unique<TestGcpQueueClientProvider>(
      std::move(dynamic_cast<TestGcpQueueClientOptions&>(options)),
      instance_client_provider, cpu_async_executor, io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
