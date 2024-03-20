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

#ifndef CPIO_CLIENT_PROVIDERS_INTERFACE_CRYPTO_CLIENT_PROVIDER_INTERFACE_H_
#define CPIO_CLIENT_PROVIDERS_INTERFACE_CRYPTO_CLIENT_PROVIDER_INTERFACE_H_

#include "src/core/interface/async_context.h"
#include "src/core/interface/service_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

namespace google::scp::cpio::client_providers {
/**
 * @brief Interface responsible for retrieving encrypter and decrypter, or
 * encrypting and decrypting payload.
 */
class CryptoClientProviderInterface {
 public:
  virtual ~CryptoClientProviderInterface() = default;

  /**
   * @brief Encrypts payload using HPKE.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual absl::Status HpkeEncrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::HpkeEncryptRequest,
                         cmrt::sdk::crypto_service::v1::HpkeEncryptResponse>&
          context) noexcept = 0;

  /**
   * @brief Decrypts payload using HPKE.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual absl::Status HpkeDecrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::HpkeDecryptRequest,
                         cmrt::sdk::crypto_service::v1::HpkeDecryptResponse>&
          context) noexcept = 0;

  /**
   * @brief Encrypts payload using AEAD.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual absl::Status AeadEncrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::AeadEncryptRequest,
                         cmrt::sdk::crypto_service::v1::AeadEncryptResponse>&
          context) noexcept = 0;

  /**
   * @brief Decrypts payload using AEAD.
   *
   * @param context context of the operation.
   * @return ExecutionResult result of the operation.
   */
  virtual absl::Status AeadDecrypt(
      core::AsyncContext<cmrt::sdk::crypto_service::v1::AeadDecryptRequest,
                         cmrt::sdk::crypto_service::v1::AeadDecryptResponse>&
          context) noexcept = 0;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_INTERFACE_CRYPTO_CLIENT_PROVIDER_INTERFACE_H_
