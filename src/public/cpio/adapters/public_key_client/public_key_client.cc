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
#include <utility>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"
#include "src/util/status_macro/status_macros.h"

using google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::PublicKeyClientProviderFactory;
using google::scp::cpio::client_providers::PublicKeyClientProviderInterface;

namespace google::scp::cpio {
absl::Status PublicKeyClient::Init() noexcept { return absl::OkStatus(); }

absl::Status PublicKeyClient::Run() noexcept { return absl::OkStatus(); }

absl::Status PublicKeyClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status PublicKeyClient::ListPublicKeys(
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
      PublicKeyClientProviderFactory::Create(
          std::move(options), &GlobalCpio::GetGlobalCpio().GetHttpClient()));
}
}  // namespace google::scp::cpio
