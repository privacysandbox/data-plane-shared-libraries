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

#include "test_aws_private_key_fetcher_provider.h"

#include <utility>

#include "src/core/interface/http_client_interface.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"

using google::scp::core::ExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::HttpRequest;
using google::scp::core::SuccessExecutionResult;

namespace google::scp::cpio::client_providers {
// In integration test, localstack API_GATEWAY doesn't support signed request.
ExecutionResult SignHttpRequestUsingV4Signer(
    std::shared_ptr<core::HttpRequest>& http_request,
    std::string_view access_key, std::string_view secret_key,
    std::string_view security_token, std::string_view region) noexcept {
  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
