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

#ifndef PUBLIC_CPIO_ADAPTERS_CRYPTO_CLIENT_CRYPTO_CLIENT_H_
#define PUBLIC_CPIO_ADAPTERS_CRYPTO_CLIENT_CRYPTO_CLIENT_H_

#include <memory>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "src/cpio/client_providers/crypto_client_provider/crypto_client_provider.h"
#include "src/cpio/client_providers/interface/crypto_client_provider_interface.h"
#include "src/public/cpio/interface/crypto_client/crypto_client_interface.h"
#include "src/public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc CryptoClientInterface
 */
class CryptoClient : public CryptoClientInterface {
 public:
  explicit CryptoClient(
      absl::Nonnull<
          std::unique_ptr<client_providers::CryptoClientProviderInterface>>
          crypto_client_provider)
      : crypto_client_provider_(std::move(crypto_client_provider)) {}

  virtual ~CryptoClient() = default;

  absl::Status Init() noexcept override;

  absl::Status Run() noexcept override;

  absl::Status Stop() noexcept override;

  absl::Status HpkeEncrypt(
      cmrt::sdk::crypto_service::v1::HpkeEncryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>
          callback) noexcept override;

  absl::Status HpkeDecrypt(
      cmrt::sdk::crypto_service::v1::HpkeDecryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>
          callback) noexcept override;

  absl::Status AeadEncrypt(
      cmrt::sdk::crypto_service::v1::AeadEncryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::AeadEncryptResponse>
          callback) noexcept override;

  absl::Status AeadDecrypt(
      cmrt::sdk::crypto_service::v1::AeadDecryptRequest request,
      Callback<cmrt::sdk::crypto_service::v1::AeadDecryptResponse>
          callback) noexcept override;

 protected:
  // Must be a pointer so it can be replaced with a mock.
  std::unique_ptr<client_providers::CryptoClientProviderInterface>
      crypto_client_provider_;
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_CRYPTO_CLIENT_CRYPTO_CLIENT_H_
