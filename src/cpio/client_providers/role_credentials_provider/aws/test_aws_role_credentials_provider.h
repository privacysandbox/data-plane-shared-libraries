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

#ifndef CPIO_CLIENT_PROVIDERS_ROLE_CREDENTIALS_PROVIDER_AWS_TEST_AWS_ROLE_CREDENTIALS_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_ROLE_CREDENTIALS_PROVIDER_AWS_TEST_AWS_ROLE_CREDENTIALS_PROVIDER_H_

#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "src/cpio/client_providers/role_credentials_provider/aws/aws_role_credentials_provider.h"
#include "src/public/cpio/test/global_cpio/test_cpio_options.h"

namespace google::scp::cpio::client_providers {
/// RoleCredentialsProviderOptions for testing on AWS.
struct TestAwsRoleCredentialsProviderOptions {
  std::string sts_endpoint_override;
  RoleCredentialsProviderOptions options;
};

/*! @copydoc AwsRoleCredentialsProvider
 */
class TestAwsRoleCredentialsProvider : public AwsRoleCredentialsProvider {
 public:
  explicit TestAwsRoleCredentialsProvider(
      TestAwsRoleCredentialsProviderOptions options,
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* cpu_async_executor,
      core::AsyncExecutorInterface* io_async_executor)
      : AwsRoleCredentialsProvider(std::move(options.options),
                                   instance_client_provider, cpu_async_executor,
                                   io_async_executor),
        sts_endpoint_override_(std::move(options.sts_endpoint_override)) {}

 protected:
  Aws::Client::ClientConfiguration CreateClientConfiguration(
      std::string_view region) noexcept override;

  std::string sts_endpoint_override_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_ROLE_CREDENTIALS_PROVIDER_AWS_TEST_AWS_ROLE_CREDENTIALS_PROVIDER_H_
