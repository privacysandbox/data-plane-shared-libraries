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

#ifndef CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AWS_TEST_AWS_PARAMETER_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AWS_TEST_AWS_PARAMETER_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/core/client/ClientConfiguration.h>

#include "src/cpio/client_providers/interface/parameter_client_provider_interface.h"
#include "src/cpio/client_providers/parameter_client_provider/aws/aws_parameter_client_provider.h"
#include "src/public/cpio/test/parameter_client/test_aws_parameter_client_options.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsParameterClientInterface
 */
class TestAwsParameterClientProvider : public AwsParameterClientProvider {
 public:
  TestAwsParameterClientProvider(
      TestAwsParameterClientOptions test_options,
      InstanceClientProviderInterface* instance_client_provider,
      core::AsyncExecutorInterface* io_async_executor)
      : AwsParameterClientProvider(test_options, /*region_code=*/"",
                                   instance_client_provider, io_async_executor),
        test_options_(std::move(test_options)) {}

 protected:
  Aws::Client::ClientConfiguration CreateClientConfiguration(
      std::string_view region) noexcept override;

  TestAwsParameterClientOptions test_options_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AWS_TEST_AWS_PARAMETER_CLIENT_PROVIDER_H_
