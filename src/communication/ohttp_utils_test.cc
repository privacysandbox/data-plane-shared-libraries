// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/communication/ohttp_utils.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/strings/escaping.h"
#include "absl/strings/match.h"
#include "quiche/common/quiche_random.h"
#include "quiche/oblivious_http/oblivious_http_client.h"

using ::testing::HasSubstr;
using ::testing::StrEq;

namespace privacy_sandbox::server_common {
namespace {

const quiche::ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id,
                                                             uint16_t kem_id,
                                                             uint16_t kdf_id,
                                                             uint16_t aead_id) {
  const auto ohttp_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id, kem_id, kdf_id, aead_id);
  EXPECT_TRUE(ohttp_key_config.ok());
  return std::move(ohttp_key_config.value());
}

std::string GetHpkePrivateKey() {
  const std::string hpke_key_hex =
      "b77431ecfa8f4cfc30d6e467aafa06944dffe28cb9dd1409e33a3045f5adc8a1";
  return absl::HexStringToBytes(hpke_key_hex);
}

std::string GetHpkePublicKey() {
  const std::string public_key =
      "6d21cfe09fbea5122f9ebc2eb2a69fcc4f06408cd54aac934f012e76fcdcef62";
  return absl::HexStringToBytes(public_key);
}

TEST(OhttpUtilsTest, ParseEncapsulatedRequest_OldRequestFormat) {
  const auto config =
      GetOhttpKeyConfig(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  const auto ohttp_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          "plaintext_payload", GetHpkePublicKey(), config);

  const std::string payload_bytes = ohttp_request->EncapsulateAndSerialize();
  auto result = ParseEncapsulatedRequest(payload_bytes);
  ASSERT_TRUE(result.ok()) << result.status();
  ASSERT_EQ(result->request_payload, ohttp_request->EncapsulateAndSerialize());
  ASSERT_EQ(result->request_label,
            quiche::ObliviousHttpHeaderKeyConfig::kOhttpRequestLabel);
}

TEST(OhttpUtilsTest, ParseEncapsulatedRequest_NewRequestFormat) {
  const auto config =
      GetOhttpKeyConfig(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  const auto ohttp_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          "plaintext_payload", GetHpkePublicKey(), config);
  const std::string payload_bytes =
      '\0' + ohttp_request->EncapsulateAndSerialize();

  auto encapsulated_request = ParseEncapsulatedRequest(payload_bytes);
  ASSERT_TRUE(encapsulated_request.ok()) << encapsulated_request.status();
  ASSERT_EQ(encapsulated_request->request_payload,
            ohttp_request->EncapsulateAndSerialize());
  ASSERT_EQ(encapsulated_request->request_label,
            kBiddingAuctionOhttpRequestLabel);
}

TEST(OhttpUtilsTest, DecryptThrowsInvalidInputOnInvalidPrimitive) {
  // B&A does not support handling OHTTP requests using this AEAD ID, so the
  // request should be rejected.
  const uint16_t invalid_hpke_aead_id = EVP_HPKE_CHACHA20_POLY1305;
  auto config = GetOhttpKeyConfig(5, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                                  EVP_HPKE_HKDF_SHA256, invalid_hpke_aead_id);
  auto ohttp_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          "plaintext_payload", GetHpkePublicKey(), config);
  const std::string payload_bytes = ohttp_request->EncapsulateAndSerialize();

  PrivateKey key;
  key.key_id = "5";
  key.private_key = "foo";

  EncapsulatedRequest request = {
      payload_bytes, quiche::ObliviousHttpHeaderKeyConfig::kOhttpRequestLabel};
  const auto result = DecryptEncapsulatedRequest(key, request);
  EXPECT_TRUE(IsInvalidArgument(result.status()));
  // Make sure the InvalidArgument error is on the AEAD ID being invalid.
  EXPECT_THAT(result.status().message(), HasSubstr("Invalid aeadID:3"));
}

TEST(OhttpUtilsTest, DecryptEncapsulatedRequestSuccess) {
  const uint8_t test_key_id = 5;
  const std::string plaintext_payload = "plaintext_payload";
  const auto config =
      GetOhttpKeyConfig(test_key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  const auto ohttp_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          plaintext_payload, GetHpkePublicKey(), config);
  const std::string payload_bytes = ohttp_request->EncapsulateAndSerialize();

  PrivateKey private_key;
  private_key.key_id = std::to_string(test_key_id);
  private_key.private_key = GetHpkePrivateKey();

  EncapsulatedRequest request = {
      payload_bytes, quiche::ObliviousHttpHeaderKeyConfig::kOhttpRequestLabel};
  const auto decrypted_req = DecryptEncapsulatedRequest(private_key, request);
  ASSERT_TRUE(decrypted_req.ok()) << decrypted_req.status();
  EXPECT_THAT(decrypted_req->GetPlaintextData(), StrEq(plaintext_payload));
}

TEST(OhttpUtilsTest, DecryptEncapsulatedRequestSuccess_NewRequestFormat) {
  const uint8_t test_key_id = 5;
  const std::string plaintext_payload = "plaintext_payload";
  const auto config =
      GetOhttpKeyConfig(test_key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);
  const auto ohttp_request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          plaintext_payload, GetHpkePublicKey(), config,
          kBiddingAuctionOhttpRequestLabel);
  const std::string payload_bytes = ohttp_request->EncapsulateAndSerialize();

  PrivateKey private_key;
  private_key.key_id = std::to_string(test_key_id);
  private_key.private_key = GetHpkePrivateKey();

  // Verify request decryption is successful, meaning
  // DecryptEncapsulatedRequest() correctly used B&A's custom request label to
  // decrypt the request.
  EncapsulatedRequest request = {payload_bytes,
                                 kBiddingAuctionOhttpRequestLabel};
  const auto decrypted_req = DecryptEncapsulatedRequest(private_key, request);
  ASSERT_TRUE(decrypted_req.ok()) << decrypted_req.status();
  EXPECT_EQ(decrypted_req->GetPlaintextData(), plaintext_payload);
}

TEST(OhttpUtilsTest, EncryptAndEncapsulateResponseSuccess) {
  // Test whether a client would be able to decrypt the encapsulated response
  // using ObliviousHttpClient, simulating a client.
  const std::string plaintext_payload = "plaintext_payload";
  const std::string response_payload = "response_payload";

  const uint8_t test_key_id = 5;
  const auto config =
      GetOhttpKeyConfig(test_key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);

  PrivateKey private_key;
  private_key.key_id = std::to_string(test_key_id);
  private_key.private_key = GetHpkePrivateKey();

  const auto http_client =
      quiche::ObliviousHttpClient::Create(GetHpkePublicKey(), config);
  auto request = http_client->CreateObliviousHttpRequest(plaintext_payload);
  auto oblivious_request_context = std::move(request.value()).ReleaseContext();
  const auto encapsulated_response = EncryptAndEncapsulateResponse(
      response_payload, private_key, oblivious_request_context,
      quiche::ObliviousHttpHeaderKeyConfig::kOhttpRequestLabel);

  const auto response = http_client->DecryptObliviousHttpResponse(
      encapsulated_response.value(), oblivious_request_context);
  ASSERT_TRUE(response.ok()) << response.status();
  EXPECT_THAT(response->GetPlaintextData(), StrEq(response_payload));
}

TEST(OhttpUtilsTest, EncryptAndEncapsulateResponseSuccess_NewFormat) {
  // Test whether a client would be able to decrypt the encapsulated response
  // using ObliviousHttpClient by simulating the client and creating the
  // request.
  const std::string plaintext_payload = "plaintext_payload";
  const std::string response_payload = "response_payload";

  const uint8_t test_key_id = 5;
  const auto config =
      GetOhttpKeyConfig(test_key_id, EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);

  PrivateKey private_key;
  private_key.key_id = std::to_string(test_key_id);
  private_key.private_key = GetHpkePrivateKey();

  const auto http_client =
      quiche::ObliviousHttpClient::Create(GetHpkePublicKey(), config);
  auto request = http_client->CreateObliviousHttpRequest(plaintext_payload);
  auto oblivious_request_context = std::move(request.value()).ReleaseContext();
  // Pass in B&A's custom request label to use the B&A's response label during
  // response encryption.
  const auto encapsulated_response = EncryptAndEncapsulateResponse(
      response_payload, private_key, oblivious_request_context,
      kBiddingAuctionOhttpRequestLabel);

  // Verify response decryption works using  B&A's custom response label.
  const absl::StatusOr<quiche::ObliviousHttpResponse> response =
      quiche::ObliviousHttpResponse::CreateClientObliviousResponse(
          std::move(encapsulated_response.value()), oblivious_request_context,
          kBiddingAuctionOhttpResponseLabel);
  ASSERT_TRUE(response.ok()) << response.status();
  ASSERT_EQ(response->GetPlaintextData(), response_payload);
}

}  // namespace
}  // namespace privacy_sandbox::server_common
