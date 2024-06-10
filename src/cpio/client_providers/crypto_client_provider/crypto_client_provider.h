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

#ifndef CPIO_CLIENT_PROVIDERS_CRYPTO_CLIENT_PROVIDER_CRYPTO_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_CRYPTO_CLIENT_PROVIDER_CRYPTO_CLIENT_PROVIDER_H_

#include <memory>
#include <utility>

#include <tink/hybrid/internal/hpke_context.h>

#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/service_interface.h"
#include "src/cpio/client_providers/interface/crypto_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/crypto_client/type_def.h"
#include "src/public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
/**
 * @copydoc CryptoClientProviderInterface
 */
class CryptoClientProvider : public CryptoClientProviderInterface {
 public:
  explicit CryptoClientProvider(CryptoClientOptions options)
      : options_(std::move(options)) {}

  absl::Status HpkeEncrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::HpkeEncryptRequest,
                         cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>&
          context) noexcept override;

  absl::Status HpkeDecrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::HpkeDecryptRequest,
                         cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>&
          context) noexcept override;

  absl::Status AeadEncrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::AeadEncryptRequest,
                         cmrt::sdk::crypto_service::v1::AeadEncryptResponse>&
          context) noexcept override;

  absl::Status AeadDecrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::AeadDecryptRequest,
                         cmrt::sdk::crypto_service::v1::AeadDecryptResponse>&
          context) noexcept override;

 protected:
  /// HpkeParams passed in from configuration which will override the default
  /// params.
  CryptoClientOptions options_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_CRYPTO_CLIENT_PROVIDER_CRYPTO_CLIENT_PROVIDER_H_
