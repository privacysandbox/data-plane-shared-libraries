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

#include "kms_client.h"

#include <memory>
#include <string_view>
#include <utility>

#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/errors.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/kms_service/v1/kms_service.pb.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::KmsClientProviderFactory;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;

namespace {
constexpr std::string_view kKmsClient = "KmsClient";
}  // namespace

namespace google::scp::cpio {

ExecutionResult KmsClient::Init() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  RoleCredentialsProviderInterface* role_credentials_provider;
  if (auto provider = cpio_->GetRoleCredentialsProvider(); !provider.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kKmsClient, kZeroUuid, execution_result,
              "Failed to get RoleCredentialsProvider.");
    return execution_result;
  } else {
    role_credentials_provider = *provider;
  }
  kms_client_provider_ = KmsClientProviderFactory::Create(
      options_, role_credentials_provider, &cpio_->GetIoAsyncExecutor());
  return SuccessExecutionResult();
}

ExecutionResult KmsClient::Run() noexcept { return SuccessExecutionResult(); }

ExecutionResult KmsClient::Stop() noexcept { return SuccessExecutionResult(); }

ExecutionResult KmsClient::Decrypt(
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context) noexcept {
  return kms_client_provider_->Decrypt(decrypt_context).ok()
             ? SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
}

std::unique_ptr<KmsClientInterface> KmsClientFactory::Create(
    KmsClientOptions options) {
  return std::make_unique<KmsClient>(std::move(options));
}
}  // namespace google::scp::cpio
