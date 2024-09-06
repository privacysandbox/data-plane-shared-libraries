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

absl::Status KmsClient::Decrypt(
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context) noexcept {
  return kms_client_provider_->Decrypt(decrypt_context);
}

std::unique_ptr<KmsClientInterface> KmsClientFactory::Create(
    KmsClientOptions /*options*/) {
  return std::make_unique<KmsClient>();
}
}  // namespace google::scp::cpio
