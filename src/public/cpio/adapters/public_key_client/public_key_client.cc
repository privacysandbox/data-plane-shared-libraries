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

#include "public_key_client.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/bind_front.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/errors.h"
#include "src/core/utils/error_utils.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::GetPublicErrorCode;
using google::scp::core::utils::ConvertToPublicExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PublicKeyClientProviderFactory;
using google::scp::cpio::client_providers::PublicKeyClientProviderInterface;

namespace {
constexpr std::string_view kPublicKeyClient = "PublicKeyClient";
}  // namespace

namespace google::scp::cpio {
ExecutionResult PublicKeyClient::CreatePublicKeyClientProvider() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  HttpClientInterface* http_client;
  if (auto client = cpio_->GetHttpClient(); !client.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to get http client.");
    return execution_result;
  } else {
    http_client = *client;
  }
  public_key_client_provider_ =
      PublicKeyClientProviderFactory::Create(*options_, http_client);
  return SuccessExecutionResult();
}

ExecutionResult PublicKeyClient::Init() noexcept {
  auto execution_result = CreatePublicKeyClientProvider();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to create PublicKeyClientProvider.");
    return ConvertToPublicExecutionResult(execution_result);
  }

  execution_result = public_key_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to initialize PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PublicKeyClient::Run() noexcept {
  auto execution_result = public_key_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to run PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

ExecutionResult PublicKeyClient::Stop() noexcept {
  auto execution_result = public_key_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kPublicKeyClient, kZeroUuid, execution_result,
              "Failed to stop PublicKeyClientProvider.");
  }
  return ConvertToPublicExecutionResult(execution_result);
}

core::ExecutionResult PublicKeyClient::ListPublicKeys(
    ListPublicKeysRequest request,
    Callback<ListPublicKeysResponse> callback) noexcept {
  return Execute<ListPublicKeysRequest, ListPublicKeysResponse>(
      absl::bind_front(&PublicKeyClientProviderInterface::ListPublicKeys,
                       public_key_client_provider_.get()),
      request, callback);
}

std::unique_ptr<PublicKeyClientInterface> PublicKeyClientFactory::Create(
    PublicKeyClientOptions options) {
  return std::make_unique<PublicKeyClient>(
      std::make_shared<PublicKeyClientOptions>(std::move(options)));
}
}  // namespace google::scp::cpio
