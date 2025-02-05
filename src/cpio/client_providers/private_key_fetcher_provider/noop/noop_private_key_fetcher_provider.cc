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
#include "src/cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::cpio::client_providers::PrivateKeyFetcherProviderInterface;
using google::scp::cpio::client_providers::PrivateKeyFetchingRequest;
using google::scp::cpio::client_providers::PrivateKeyFetchingResponse;

namespace {
class NoopPrivateKeyFetcherProvider
    : public PrivateKeyFetcherProviderInterface {
 public:
  ExecutionResult Init() noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }
  ExecutionResult Run() noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }
  ExecutionResult Stop() noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }

  ExecutionResult FetchPrivateKey(
      AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
      /* context */) noexcept override {
    return FailureExecutionResult(SC_UNKNOWN);
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::Nonnull<std::unique_ptr<PrivateKeyFetcherProviderInterface>>
PrivateKeyFetcherProviderFactory::Create(
    absl::Nonnull<core::HttpClientInterface*> /* http_client */,
    absl::Nonnull<
        RoleCredentialsProviderInterface*> /* role_credentials_provider */,
    absl::Nonnull<AuthTokenProviderInterface*> /* auth_token_provider */,
    privacy_sandbox::server_common::log::PSLogContext& /* log_context */) {
  return std::make_unique<NoopPrivateKeyFetcherProvider>();
}
}  // namespace google::scp::cpio::client_providers
