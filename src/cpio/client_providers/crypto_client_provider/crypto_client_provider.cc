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

#include "crypto_client_provider.h"

#include <cctype>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <tink/aead.h>
#include <tink/binary_keyset_reader.h>
#include <tink/cleartext_keyset_handle.h>
#include <tink/hybrid/internal/hpke_context.h>
#include <tink/keyset_handle.h>
#include <tink/subtle/aes_gcm_boringssl.h>
#include <tink/util/secret_data.h>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "proto/hpke.pb.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/service_interface.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/crypto_service/v1/crypto_service.pb.h"

#include "error_codes.h"

using crypto::tink::Aead;
using crypto::tink::BinaryKeysetReader;
using crypto::tink::CleartextKeysetHandle;
using crypto::tink::KeysetHandle;
using crypto::tink::internal::ConcatenatePayload;
using crypto::tink::internal::HpkeContext;
using crypto::tink::internal::SplitPayload;
using crypto::tink::subtle::AesGcmBoringSsl;
using crypto::tink::util::SecretData;
using crypto::tink::util::SecretDataAsStringView;
using crypto::tink::util::SecretDataFromStringView;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::AeadEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeAead;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeDecryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptRequest;
using google::cmrt::sdk::crypto_service::v1::HpkeEncryptResponse;
using google::cmrt::sdk::crypto_service::v1::HpkeKdf;
using google::cmrt::sdk::crypto_service::v1::HpkeKem;
using google::cmrt::sdk::crypto_service::v1::HpkeParams;
using google::cmrt::sdk::crypto_service::v1::SecretLength;
using google::crypto::tink::HpkePrivateKey;
using google::protobuf::Any;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::PublicPrivateKeyPairId;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_AEAD_DECRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_AEAD_ENCRYPT_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CANNOT_READ_BINARY_KEY_SET_FROM_PRIVATE_KEY;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_INVALID_KEYSET_SIZE;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PRIVATE_KEY_FAILED;
using google::scp::core::errors::SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED;
using google::scp::core::errors::
    SC_CRYPTO_CLIENT_PROVIDER_SPLIT_CIPHERTEXT_FAILED;
using google::scp::core::utils::Base64Decode;

namespace google::scp::cpio::client_providers {
namespace tink = ::crypto::tink::internal;

namespace {
/// Filename for logging errors
constexpr std::string_view kCryptoClientProvider = "CryptoClientProvider";
constexpr std::string_view kDefaultExporterContext = "aead key";

/// Default HpkeParams if it is not configured or specified from the request.
const tink::HpkeParams kDefaultHpkeParams = {tink::HpkeKem::kX25519HkdfSha256,
                                             tink::HpkeKdf::kHkdfSha256,
                                             tink::HpkeAead::kChaCha20Poly1305};
// Static duration maps are heap allocated to avoid destructor call.
/// Map from HpkeKem to Tink HpkeKem.
const auto& kHpkeKemMap = *new absl::flat_hash_map<HpkeKem, tink::HpkeKem>{
    {HpkeKem::DHKEM_X25519_HKDF_SHA256, tink::HpkeKem::kX25519HkdfSha256},
    {HpkeKem::KEM_UNSPECIFIED, tink::HpkeKem::kUnknownKem},
};
/// Map from HpkeKdf to Tink HpkeKdf.
const auto& kHpkeKdfMap = *new absl::flat_hash_map<HpkeKdf, tink::HpkeKdf>{
    {HpkeKdf::HKDF_SHA256, tink::HpkeKdf::kHkdfSha256},
    {HpkeKdf::KDF_UNSPECIFIED, tink::HpkeKdf::kUnknownKdf},
};
/// Map from HpkeAead to Tink HpkeAead.
const auto& kHpkeAeadMap = *new absl::flat_hash_map<HpkeAead, tink::HpkeAead>{
    {HpkeAead::AES_128_GCM, tink::HpkeAead::kAes128Gcm},
    {HpkeAead::AES_256_GCM, tink::HpkeAead::kAes256Gcm},
    {HpkeAead::CHACHA20_POLY1305, tink::HpkeAead::kChaCha20Poly1305},
    {HpkeAead::AEAD_UNSPECIFIED, tink::HpkeAead::kUnknownAead},
};
}  // namespace

/**
 * @brief Gets configured HpkeParams if it is set otherwise gets default
 * HpkeParams.
 *
 * @param hpke_params_config configured HpkeParams.
 * @return HpkeParams params which is already set.
 */
tink::HpkeParams GetExistingHpkeParams(const HpkeParams& hpke_params_config) {
  tink::HpkeParams hpke_params;
  hpke_params.kem = kHpkeKemMap.at(hpke_params_config.kem());
  hpke_params.kdf = kHpkeKdfMap.at(hpke_params_config.kdf());
  hpke_params.aead = kHpkeAeadMap.at(hpke_params_config.aead());
  if (hpke_params.kem == tink::HpkeKem::kUnknownKem) {
    hpke_params.kem = kDefaultHpkeParams.kem;
  }
  if (hpke_params.kdf == tink::HpkeKdf::kUnknownKdf) {
    hpke_params.kdf = kDefaultHpkeParams.kdf;
  }
  if (hpke_params.aead == tink::HpkeAead::kUnknownAead) {
    hpke_params.aead = kDefaultHpkeParams.aead;
  }
  return hpke_params;
}

/**
 * @brief Converts HpkeParams we have to Tink' HpkeParams. hpke_params_proto
 * will override the default HpkeParams or what we've configured.
 *
 * @param hpke_params_proto HpkeParams from input request.
 * @param existing_hpke_params the default HpkeParams or what we've configured.
 * @return HpkeParams HpkeParams to be used by Tink.
 */
tink::HpkeParams ToHpkeParams(const HpkeParams& hpke_params_proto,
                              const tink::HpkeParams& existing_hpke_params) {
  tink::HpkeParams hpke_params;
  hpke_params.kem = kHpkeKemMap.at(hpke_params_proto.kem());
  hpke_params.kdf = kHpkeKdfMap.at(hpke_params_proto.kdf());
  hpke_params.aead = kHpkeAeadMap.at(hpke_params_proto.aead());
  if (hpke_params.kem == tink::HpkeKem::kUnknownKem) {
    hpke_params.kem = existing_hpke_params.kem;
  }
  if (hpke_params.kdf == tink::HpkeKdf::kUnknownKdf) {
    hpke_params.kdf = existing_hpke_params.kdf;
  }
  if (hpke_params.aead == tink::HpkeAead::kUnknownAead) {
    hpke_params.aead = existing_hpke_params.aead;
  }
  return hpke_params;
}

/**
 * @brief Gets the Secret Length.
 *
 * @param secret_length SecretLength enum.
 * @return size_t the length.
 */
size_t GetSecretLength(const SecretLength& secret_length) {
  switch (secret_length) {
    case SecretLength::SECRET_LENGTH_32_BYTES:
      return 32;
    case SecretLength::SECRET_LENGTH_16_BYTES:
      [[fallthrough]];
    default:
      return 16;
  }
}

/**
 * @brief Gets a random number between 0 and size-1.
 *
 * @param size specified size.
 * @return uint64_t the random number.
 */
uint64_t GetRandomNumber(int size) {
  static std::random_device random_device_local;
  static std::mt19937 random_generator(random_device_local());
  std::uniform_int_distribution<uint64_t> distribution;

  return distribution(random_generator) % size;
}

absl::Status CryptoClientProvider::HpkeEncrypt(
    AsyncContext<HpkeEncryptRequest, HpkeEncryptResponse>&
        encrypt_context) noexcept {
  std::string decoded_key;
  Base64Decode(encrypt_context.request->public_key().public_key(), decoded_key);
  auto cipher = HpkeContext::SetupSender(
      ToHpkeParams(encrypt_context.request->hpke_params(),
                   GetExistingHpkeParams(options_.hpke_params)),
      decoded_key, "" /*Empty application info*/);

  if (!cipher.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, encrypt_context, execution_result,
                      "Hpke encryption failed with error %s.",
                      cipher.status().ToString().c_str());
    encrypt_context.Finish(execution_result);
    return cipher.status();
  }

  auto ciphertext = (*cipher)->Seal(encrypt_context.request->payload(),
                                    encrypt_context.request->shared_info());
  if (!ciphertext.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_HPKE_ENCRYPT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, encrypt_context, execution_result,
                      "Hpke encryption failed with error %s.",
                      ciphertext.status().ToString().c_str());
    encrypt_context.Finish(execution_result);
    return ciphertext.status();
  }

  encrypt_context.response = std::make_shared<HpkeEncryptResponse>();
  if (encrypt_context.request->is_bidirectional()) {
    auto secret = (*cipher)->Export(
        encrypt_context.request->exporter_context().empty()
            ? kDefaultExporterContext
            : encrypt_context.request->exporter_context(),
        GetSecretLength(encrypt_context.request->secret_length()));
    if (!secret.ok()) {
      auto execution_result = FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED);
      SCP_ERROR_CONTEXT(kCryptoClientProvider, encrypt_context,
                        execution_result,
                        "Hpke encryption failed with error %s.",
                        secret.status().ToString().c_str());
      encrypt_context.Finish(execution_result);
      return secret.status();
    }
    encrypt_context.response->set_secret(
        std::string(SecretDataAsStringView((*secret))));
  }

  encrypt_context.response->mutable_encrypted_data()->set_key_id(
      encrypt_context.request->public_key().key_id());
  encrypt_context.response->mutable_encrypted_data()->set_ciphertext(
      ConcatenatePayload((*cipher)->EncapsulatedKey(), *ciphertext));

  encrypt_context.Finish(SuccessExecutionResult());

  return absl::OkStatus();
}

absl::Status CryptoClientProvider::HpkeDecrypt(
    AsyncContext<HpkeDecryptRequest, HpkeDecryptResponse>&
        decrypt_context) noexcept {
  std::string decoded_key;
  auto execution_result = Base64Decode(
      decrypt_context.request->private_key().private_key(), decoded_key);
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  auto keyset_reader = BinaryKeysetReader::New(decoded_key);
  if (!keyset_reader.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_READ_BINARY_KEY_SET_FROM_PRIVATE_KEY);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error %s.",
                      keyset_reader.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return keyset_reader.status();
  }

  auto keyset_handle = CleartextKeysetHandle::Read(std::move(*keyset_reader));
  if (!keyset_handle.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CANNOT_CREATE_KEYSET_HANDLE);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error %s.",
                      keyset_handle.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return keyset_handle.status();
  }

  auto keyset = CleartextKeysetHandle::GetKeyset(*keyset_handle.value());
  if (keyset.key_size() != 1) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_INVALID_KEYSET_SIZE);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error.");
    decrypt_context.Finish(execution_result);
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }
  auto hpke_params = ToHpkeParams(decrypt_context.request->hpke_params(),
                                  GetExistingHpkeParams(options_.hpke_params));
  auto splitted_ciphertext = SplitPayload(
      hpke_params.kem, decrypt_context.request->encrypted_data().ciphertext());
  if (!splitted_ciphertext.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_SPLIT_CIPHERTEXT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error %s.",
                      splitted_ciphertext.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return splitted_ciphertext.status();
  }

  HpkePrivateKey private_key;
  if (!private_key.ParseFromString(keyset.key(0).key_data().value())) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_PARSE_HPKE_PRIVATE_KEY_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error.");
    decrypt_context.Finish(execution_result);
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        execution_result.status_code));
  }

  auto cipher = HpkeContext::SetupRecipient(
      hpke_params, SecretDataFromStringView(private_key.private_key()),
      splitted_ciphertext->encapsulated_key, "" /*Empty application info*/);

  if (!cipher.ok()) {
    auto execution_result = FailureExecutionResult(
        SC_CRYPTO_CLIENT_PROVIDER_CREATE_HPKE_CONTEXT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error %s.",
                      cipher.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return cipher.status();
  }

  auto payload = (*cipher)->Open(splitted_ciphertext->ciphertext,
                                 decrypt_context.request->shared_info());
  if (!payload.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_HPKE_DECRYPT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context, execution_result,
                      "Hpke decryption failed with error %s.",
                      payload.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return payload.status();
  }

  decrypt_context.response = std::make_shared<HpkeDecryptResponse>();
  if (decrypt_context.request->is_bidirectional()) {
    auto secret = (*cipher)->Export(
        decrypt_context.request->exporter_context().empty()
            ? kDefaultExporterContext
            : decrypt_context.request->exporter_context(),
        GetSecretLength(decrypt_context.request->secret_length()));
    if (!secret.ok()) {
      auto execution_result = FailureExecutionResult(
          SC_CRYPTO_CLIENT_PROVIDER_SECRET_EXPORT_FAILED);
      SCP_ERROR_CONTEXT(kCryptoClientProvider, decrypt_context,
                        execution_result,
                        "Hpke decryption failed with error %s.",
                        secret.status().ToString().c_str());
      decrypt_context.Finish(execution_result);
      return secret.status();
    }
    decrypt_context.response->set_secret(
        std::string(SecretDataAsStringView(*secret)));
  }

  decrypt_context.response->set_payload(*payload);
  decrypt_context.Finish(SuccessExecutionResult());

  return absl::OkStatus();
}

absl::Status CryptoClientProvider::AeadEncrypt(
    AsyncContext<AeadEncryptRequest, AeadEncryptResponse>& context) noexcept {
  SecretData key = SecretDataFromStringView(context.request->secret());
  auto cipher = AesGcmBoringSsl::New(key);
  if (!cipher.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, context, execution_result,
                      "Aead encryption failed with error %s.",
                      cipher.status().ToString().c_str());
    context.Finish(execution_result);
    return cipher.status();
  }
  auto ciphertext = (*cipher)->Encrypt(context.request->payload(),
                                       context.request->shared_info());
  if (!ciphertext.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_AEAD_ENCRYPT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, context, execution_result,
                      "Aead encryption failed with error %s.",
                      ciphertext.status().ToString().c_str());
    context.Finish(execution_result);
    return ciphertext.status();
  }
  context.response = std::make_shared<AeadEncryptResponse>();
  context.response->mutable_encrypted_data()->set_ciphertext((*ciphertext));
  context.Finish(SuccessExecutionResult());
  return absl::OkStatus();
}

absl::Status CryptoClientProvider::AeadDecrypt(
    AsyncContext<AeadDecryptRequest, AeadDecryptResponse>& context) noexcept {
  SecretData key = SecretDataFromStringView(context.request->secret());
  auto cipher = AesGcmBoringSsl::New(key);
  if (!cipher.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, context, execution_result,
                      "Aead decryption failed with error %s.",
                      cipher.status().ToString().c_str());
    context.Finish(execution_result);
    return cipher.status();
  }
  auto payload =
      (*cipher)->Decrypt(context.request->encrypted_data().ciphertext(),
                         context.request->shared_info());
  if (!payload.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_CRYPTO_CLIENT_PROVIDER_AEAD_DECRYPT_FAILED);
    SCP_ERROR_CONTEXT(kCryptoClientProvider, context, execution_result,
                      "Aead decryption failed with error %s.",
                      payload.status().ToString().c_str());
    context.Finish(execution_result);
    return payload.status();
  }
  context.response = std::make_shared<AeadDecryptResponse>();
  context.response->set_payload((*payload));
  context.Finish(SuccessExecutionResult());
  return absl::OkStatus();
}
}  // namespace google::scp::cpio::client_providers
