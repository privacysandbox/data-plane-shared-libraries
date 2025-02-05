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
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::
    GetSessionTokenForTargetAudienceRequest;
using google::scp::cpio::client_providers::GetSessionTokenRequest;
using google::scp::cpio::client_providers::GetSessionTokenResponse;

namespace {
class NoopAuthTokenProvider : public AuthTokenProviderInterface {
 public:
  ExecutionResult GetSessionToken(
      AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>&
      /* get_role_credentials_context */) noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult GetSessionTokenForTargetAudience(
      AsyncContext<GetSessionTokenForTargetAudienceRequest,
                   GetSessionTokenResponse>& /* get_token_context */) noexcept
      override {
    return FailureExecutionResult(SC_UNKNOWN);
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::Nonnull<std::unique_ptr<AuthTokenProviderInterface>>
AuthTokenProviderFactory::Create(
    absl::Nonnull<core::HttpClientInterface*> /* http1_client */) {
  return std::make_unique<NoopAuthTokenProvider>();
}
}  // namespace google::scp::cpio::client_providers
