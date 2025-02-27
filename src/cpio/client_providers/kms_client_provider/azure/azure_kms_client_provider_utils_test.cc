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

#include "azure_kms_client_provider_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "src/core/interface/http_types.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::ExecutionResult;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;

namespace {
constexpr char kKeyId[] = "123";
constexpr char kPrivateKeyBaseUri[] = "http://localhost.test:8000";
}  // namespace

namespace google::scp::cpio::client_providers::test {

TEST(AzureKmsClientProviderUtilsTest, GenerateWrappingKey) {
  std::cout << "Starting test GenerateWrappingKey..." << std::endl;

  auto wrapping_key =
      AzureKmsClientProviderUtils::GenerateWrappingKey().value();

  ASSERT_NE(wrapping_key.first, nullptr) << "Private key is null";
  ASSERT_TRUE(AzureKmsClientProviderUtils::isPrivate(wrapping_key.first));
  ASSERT_NE(wrapping_key.second, nullptr) << "Public key is null";
  ASSERT_FALSE(AzureKmsClientProviderUtils::isPrivate(wrapping_key.second));
  std::cout << "GenerateWrappingKey generated keys" << std::endl;

  std::string pem =
      AzureKmsClientProviderUtils::EvpPkeyToPem(wrapping_key.first).value();
  std::cout << "GenerateWrappingKey PEM: " << pem << std::endl;

  // Add the constant to avoid the key detection precommit
  auto to_test = std::string("-----") + std::string("BEGIN PRIVATE") +
                 std::string(" KEY-----");
  ASSERT_EQ(pem.find(to_test), 0) << "Private key PEM header not found";
  std::cout << "Private key found" << std::endl;

  pem = AzureKmsClientProviderUtils::EvpPkeyToPem(wrapping_key.second).value();
  ASSERT_EQ(pem.find("-----BEGIN PUBLIC KEY-----"), 0)
      << "Public key PEM header not found";
  std::cout << "Public key found" << std::endl;

  std::cout << "Test GenerateWrappingKey completed successfully." << std::endl;
}

TEST(AzureKmsClientProviderUtilsTest, WrapUnwrap) {
  // Generate wrapping key
  auto wrapping_key_pair =
      AzureKmsClientProviderUtils::GenerateWrappingKey().value();
  auto public_key = wrapping_key_pair.second;
  auto private_key = wrapping_key_pair.first;
  std::cout << "key pair generated" << std::endl;
  // Original message to encrypt
  const std::string payload = "payload";

  // Encrypt the payload
  const auto cipher =
      AzureKmsClientProviderUtils::KeyWrap(public_key, payload).value();
  ASSERT_FALSE(cipher.empty());

  // Decrypt the encrypted message
  std::string decrypted =
      AzureKmsClientProviderUtils::KeyUnwrap(private_key, cipher).value();

  // Assert that decrypted message matches original payload
  ASSERT_EQ(decrypted, payload);
  std::cout << decrypted << " matches " << payload << std::endl;
}

TEST(AzureKmsClientProviderUtilsTest, GenerateWrappingKeyHash) {
  auto public_pem_key = AzureKmsClientProviderUtils::GetTestPemPublicWrapKey();
  std::cout << "Test GenerateWrappingKeyHash PEM key: " << public_pem_key
            << std::endl;
  auto public_key =
      AzureKmsClientProviderUtils::PemToEvpPkey(public_pem_key).value();

  auto hex_hash =
      AzureKmsClientProviderUtils::CreateHexHashOnKey(public_key).value();
  std::cout << "##################HASH: " << hex_hash << std::endl;
  ASSERT_EQ(hex_hash.size(), 64);
  ASSERT_EQ(hex_hash,
            "36b03dab8e8751b26d9b33fa2fa1296f823a238ef3dd604f758a4aff5b2b41d0");
}

}  // namespace google::scp::cpio::client_providers::test
