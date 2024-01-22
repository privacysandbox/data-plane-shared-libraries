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

#ifndef CPIO_CLIENT_PROVIDERS_AUTO_SCALING_CLIENT_PROVIDER_TEST_AWS_TEST_AWS_AUTO_SCALING_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_AUTO_SCALING_CLIENT_PROVIDER_TEST_AWS_TEST_AWS_AUTO_SCALING_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include "cpio/client_providers/auto_scaling_client_provider/src/aws/aws_auto_scaling_client_provider.h"

namespace google::scp::cpio::client_providers {
/// AutoScalingClientOptions for testing on AWS.
struct TestAwsAutoScalingClientOptions : public AutoScalingClientOptions {
  std::shared_ptr<std::string> auto_scaling_client_endpoint_override;
};

/*! @copydoc AwsAutoScalingClientProvider
 */
class TestAwsAutoScalingClientProvider : public AwsAutoScalingClientProvider {
 public:
  explicit TestAwsAutoScalingClientProvider(
      TestAwsAutoScalingClientOptions test_options,
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* io_async_executor)
      : AwsAutoScalingClientProvider(test_options, instance_client_provider,
                                     io_async_executor),
        test_options_(std::move(test_options)) {}

 private:
  Aws::Client::ClientConfiguration CreateClientConfiguration(
      std::string_view region) noexcept override;

  TestAwsAutoScalingClientOptions test_options_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_AUTO_SCALING_CLIENT_PROVIDER_TEST_AWS_TEST_AWS_AUTO_SCALING_CLIENT_PROVIDER_H_
