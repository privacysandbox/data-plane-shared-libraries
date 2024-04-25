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

#include "private_key_client.h"

#include <memory>
#include <utility>

#include "absl/functional/bind_front.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/auth_token_provider_interface.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"
#include "src/util/status_macro/status_macros.h"

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PrivateKeyClientProviderFactory;
using google::scp::cpio::client_providers::PrivateKeyClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;

namespace google::scp::cpio {
absl::Status PrivateKeyClient::Init() noexcept {
  PS_ASSIGN_OR_RETURN(
      RoleCredentialsProviderInterface * role_credentials_provider,
      GlobalCpio::GetGlobalCpio().GetRoleCredentialsProvider());
  private_key_client_provider_ = PrivateKeyClientProviderFactory::Create(
      options_, &GlobalCpio::GetGlobalCpio().GetHttpClient(),
      role_credentials_provider,
      &GlobalCpio::GetGlobalCpio().GetAuthTokenProvider(),
      &GlobalCpio::GetGlobalCpio().GetIoAsyncExecutor());
  return absl::OkStatus();
}

absl::Status PrivateKeyClient::Run() noexcept { return absl::OkStatus(); }

absl::Status PrivateKeyClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status PrivateKeyClient::ListPrivateKeys(
    ListPrivateKeysRequest request,
    Callback<ListPrivateKeysResponse> callback) noexcept {
  return Execute<ListPrivateKeysRequest, ListPrivateKeysResponse>(
      absl::bind_front(&PrivateKeyClientProviderInterface::ListPrivateKeys,
                       private_key_client_provider_.get()),
      request, callback);
}

std::unique_ptr<PrivateKeyClientInterface> PrivateKeyClientFactory::Create(
    PrivateKeyClientOptions options) {
  return std::make_unique<PrivateKeyClient>(std::move(options));
}
}  // namespace google::scp::cpio
