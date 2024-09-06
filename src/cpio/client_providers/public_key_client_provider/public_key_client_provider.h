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

#ifndef CPIO_CLIENT_PROVIDERS_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEY_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEY_CLIENT_PROVIDER_H_

#include <memory>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/http_types.h"
#include "src/cpio/client_providers/interface/public_key_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PublicKeyClientProviderInterface
 */
class PublicKeyClientProvider : public PublicKeyClientProviderInterface {
 public:
  virtual ~PublicKeyClientProvider() = default;

  explicit PublicKeyClientProvider(
      PublicKeyClientOptions public_key_client_options,
      absl::Nonnull<core::HttpClientInterface*> http_client)
      : http_client_(http_client),
        public_key_client_options_(std::move(public_key_client_options)) {}

  absl::Status ListPublicKeys(
      core::AsyncContext<
          cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
          cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&
          context) noexcept override;

 protected:
  /**
   * @brief Triggered when ListPublicKeysRequest arrives.
   *
   * @param context async execution context.
   */
  virtual void OnListPublicKeys(
      core::AsyncContext<google::protobuf::Any, google::protobuf::Any>
          context) noexcept;

  /**
   * @brief Is called after http client PerformRequest() is completed.
   *
   * @param public_key_fetching_context public key fetching context.
   * @param http_client_context http client operation context.
   * @param got_success_result whether got success result.
   * @param failed_counters how many uri requests have being failed.
   */
  void OnPerformRequestCallback(
      core::AsyncContext<
          cmrt::sdk::public_key_service::v1::ListPublicKeysRequest,
          cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>&
          public_key_fetching_context,
      core::AsyncContext<core::HttpRequest, core::HttpResponse>&
          http_client_context,
      std::shared_ptr<std::atomic<bool>> got_success_result,
      std::shared_ptr<std::atomic<size_t>> failed_counters) noexcept;

  /// HttpClient for issuing HTTP actions.
  core::HttpClientInterface* http_client_;

  /// Configurations for PublicKeyClient.
  PublicKeyClientOptions public_key_client_options_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEY_CLIENT_PROVIDER_H_
