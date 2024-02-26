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

#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_AWS_TEST_AWS_PRIVATE_KEY_FETCHER_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_AWS_TEST_AWS_PRIVATE_KEY_FETCHER_PROVIDER_H_

#include <memory>
#include <string>

#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/cpio/client_providers/private_key_fetcher_provider/aws/aws_private_key_fetcher_provider.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc AwsPrivateKeyFetcherProvider
 */
class TestAwsPrivateKeyFetcherProvider : public AwsPrivateKeyFetcherProvider {
 public:
  /**
   * @brief Constructs a new AWS Private Key Fetching Client Provider object.
   *
   * @param http_client http client to issue http requests.
   * @param credentials_provider credentials provider.
   * service.
   */
  TestAwsPrivateKeyFetcherProvider(
      core::HttpClientInterface* http_client,
      RoleCredentialsProviderInterface* role_credentials_provider)
      : AwsPrivateKeyFetcherProvider(http_client, role_credentials_provider) {}

 protected:
  core::ExecutionResult SignHttpRequestUsingV4Signer(
      std::shared_ptr<core::HttpRequest>& http_request,
      std::string_view access_key, std::string_view secret_key,
      std::string_view security_token,
      std::string_view region) noexcept override;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_AWS_TEST_AWS_PRIVATE_KEY_FETCHER_PROVIDER_H_
