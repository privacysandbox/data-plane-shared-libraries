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
#include <utility>

#include "absl/status/status.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/cpio/proto/kms_service/v1/kms_service.pb.h"
#include "src/util/status_macro/status_macros.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::KmsClientProviderFactory;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;

namespace google::scp::cpio {

<<<<<<< HEAD
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
=======
absl::Status KmsClient::Init() noexcept {
  PS_ASSIGN_OR_RETURN(
      RoleCredentialsProviderInterface * role_credentials_provider,
      GlobalCpio::GetGlobalCpio().GetRoleCredentialsProvider());
  kms_client_provider_ = KmsClientProviderFactory::Create(
      role_credentials_provider,
      &GlobalCpio::GetGlobalCpio().GetIoAsyncExecutor());
  return absl::OkStatus();
}

absl::Status KmsClient::Run() noexcept { return absl::OkStatus(); }

absl::Status KmsClient::Stop() noexcept { return absl::OkStatus(); }
>>>>>>> upstream-3e92e75-3.10.0

absl::Status KmsClient::Decrypt(
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context) noexcept {
  return kms_client_provider_->Decrypt(decrypt_context).ok()
             ? SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
}

std::unique_ptr<KmsClientInterface> KmsClientFactory::Create(
<<<<<<< HEAD
    KmsClientOptions options) {
  return std::make_unique<KmsClient>(std::move(options));
=======
    KmsClientOptions /*options*/) {
  return std::make_unique<KmsClient>();
>>>>>>> upstream-3e92e75-3.10.0
}
}  // namespace google::scp::cpio
