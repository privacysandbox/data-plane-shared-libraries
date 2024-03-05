/*
 * Portions Copyright (c) Microsoft Corporation
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

#include "azure_private_key_fetcher_provider_utils.h"

#include "azure/attestation/src/attestation.h"

using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::Uri;

namespace google::scp::cpio::client_providers {

void AzurePrivateKeyFetchingClientUtils::CreateHttpRequest(
    const PrivateKeyFetchingRequest& request, HttpRequest& http_request) {
  const auto& base_uri =
      request.key_vending_endpoint->private_key_vending_service_endpoint;
  http_request.method = HttpMethod::POST;

  http_request.path = std::make_shared<Uri>(base_uri);
  const auto report =
      hasSnp() ? fetchSnpAttestation() : fetchFakeSnpAttestation();
  CHECK(report.has_value()) << "Failed to get attestation report";
  http_request.body = core::BytesBuffer(nlohmann::json(report.value()).dump());
}

/**
 * @brief Generate a new wrapping key
*/
RSA* AzurePrivateKeyFetchingClientUtils::GenerateWrappingKey() {
  RSA* rsa = RSA_new();
  BIGNUM* e = BN_new();

  BN_set_word(e, RSA_F4);
  RSA_generate_key_ex(rsa, 2048, e, NULL);

  BN_free(e);
  return rsa;
}

/**
 * @brief Wrap a key using RSA OAEP
   *
   * @param wrappingKey RSA public key used to wrap a key.
   * @param key         Key in PEM format to wrap.
*/
std::vector<unsigned char> AzurePrivateKeyFetchingClientUtils::KeyWrap(
    RSA* wrappingKey, const std::string& key) {
  std::vector<unsigned char> encrypted(RSA_size(wrappingKey));
  int encrypted_len = RSA_public_encrypt(
      key.size(), reinterpret_cast<const unsigned char*>(key.c_str()),
      encrypted.data(), wrappingKey, RSA_PKCS1_OAEP_PADDING);
  encrypted.resize(encrypted_len);
  return encrypted;
}

/**
 * @brief Unwrap a key using RSA OAEP
   *
   * @param wrappingKey RSA private key used to unwrap a key.
   * @param encrypted   Wrapped key to unwrap.
*/
std::string AzurePrivateKeyFetchingClientUtils::KeyUnwrap(
    RSA* wrappingKey, const std::vector<unsigned char>& encrypted) {
  std::vector<unsigned char> decrypted(RSA_size(wrappingKey));
  int decrypted_len =
      RSA_private_decrypt(encrypted.size(), encrypted.data(), decrypted.data(),
                          wrappingKey, RSA_PKCS1_OAEP_PADDING);
  decrypted.resize(decrypted_len);
  return std::string(decrypted.begin(), decrypted.end());
}

}  // namespace google::scp::cpio::client_providers
