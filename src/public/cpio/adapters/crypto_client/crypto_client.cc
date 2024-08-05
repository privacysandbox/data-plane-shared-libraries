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

#include "crypto_client.h"

#include <memory>

#include "absl/functional/bind_front.h"
#include "absl/status/status.h"
#include "src/core/interface/async_context.h"
<<<<<<< HEAD
#include "src/core/interface/errors.h"
#include "src/core/utils/error_utils.h"
#include "src/public/core/interface/execution_result.h"
=======
>>>>>>> upstream-3e92e75-3.10.0
#include "src/public/cpio/adapters/common/adapter_utils.h"
#include "src/public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::scp::cpio::client_providers::CryptoClientProviderInterface;

namespace google::scp::cpio {
<<<<<<< HEAD
ExecutionResult CryptoClient::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult CryptoClient::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult CryptoClient::Stop() noexcept {
  return SuccessExecutionResult();
}

core::ExecutionResult CryptoClient::HpkeEncrypt(
    HpkeEncryptRequest request,
    Callback<HpkeEncryptResponse> callback) noexcept {
  return Execute<HpkeEncryptRequest, HpkeEncryptResponse>(
             absl::bind_front(&CryptoClientProviderInterface::HpkeEncrypt,
                              crypto_client_provider_.get()),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
=======
absl::Status CryptoClient::Init() noexcept { return absl::OkStatus(); }

absl::Status CryptoClient::Run() noexcept { return absl::OkStatus(); }

absl::Status CryptoClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status CryptoClient::HpkeEncrypt(
    HpkeEncryptRequest request,
    Callback<HpkeEncryptResponse> callback) noexcept {
  return Execute<HpkeEncryptRequest, HpkeEncryptResponse>(
      absl::bind_front(&CryptoClientProviderInterface::HpkeEncrypt,
                       crypto_client_provider_.get()),
      request, callback);
>>>>>>> upstream-3e92e75-3.10.0
}

absl::Status CryptoClient::HpkeDecrypt(
    HpkeDecryptRequest request,
    Callback<HpkeDecryptResponse> callback) noexcept {
  return Execute<HpkeDecryptRequest, HpkeDecryptResponse>(
<<<<<<< HEAD
             absl::bind_front(&CryptoClientProviderInterface::HpkeDecrypt,
                              crypto_client_provider_.get()),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
=======
      absl::bind_front(&CryptoClientProviderInterface::HpkeDecrypt,
                       crypto_client_provider_.get()),
      request, callback);
>>>>>>> upstream-3e92e75-3.10.0
}

absl::Status CryptoClient::AeadEncrypt(
    AeadEncryptRequest request,
    Callback<AeadEncryptResponse> callback) noexcept {
  return Execute<AeadEncryptRequest, AeadEncryptResponse>(
<<<<<<< HEAD
             absl::bind_front(&CryptoClientProviderInterface::AeadEncrypt,
                              crypto_client_provider_.get()),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
=======
      absl::bind_front(&CryptoClientProviderInterface::AeadEncrypt,
                       crypto_client_provider_.get()),
      request, callback);
>>>>>>> upstream-3e92e75-3.10.0
}

absl::Status CryptoClient::AeadDecrypt(
    AeadDecryptRequest request,
    Callback<AeadDecryptResponse> callback) noexcept {
  return Execute<AeadDecryptRequest, AeadDecryptResponse>(
<<<<<<< HEAD
             absl::bind_front(&CryptoClientProviderInterface::AeadDecrypt,
                              crypto_client_provider_.get()),
             request, callback)
                 .ok()
             ? core::SuccessExecutionResult()
             : core::FailureExecutionResult(SC_UNKNOWN);
=======
      absl::bind_front(&CryptoClientProviderInterface::AeadDecrypt,
                       crypto_client_provider_.get()),
      request, callback);
>>>>>>> upstream-3e92e75-3.10.0
}

std::unique_ptr<CryptoClientInterface> CryptoClientFactory::Create(
    CryptoClientOptions options) {
<<<<<<< HEAD
  return std::make_unique<CryptoClient>(std::move(options));
=======
  return std::make_unique<CryptoClient>(
      std::make_unique<client_providers::CryptoClientProvider>(
          std::move(options)));
>>>>>>> upstream-3e92e75-3.10.0
}
}  // namespace google::scp::cpio
