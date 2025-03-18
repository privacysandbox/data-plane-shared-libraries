// Portions Copyright (c) Microsoft Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

#include <nlohmann/json.hpp>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/strings/escaping.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "quiche/common/quiche_random.h"
#include "quiche/oblivious_http/oblivious_http_client.h"
#include "src/communication/ohttp_utils.h"
#include "src/concurrent/event_engine_executor.h"
#include "src/core/lib/event_engine/default_event_engine.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/encryption/key_fetcher/fake_key_fetcher_manager.h"
#include "src/public/cpio/interface/cpio.h"

using google::scp::core::AsyncContext;
using google::scp::cpio::client_providers::GetSessionTokenRequest;
using google::scp::cpio::client_providers::GetSessionTokenResponse;
using google::scp::cpio::client_providers::GlobalCpio;

/*
This tool encrypts the stdin input payload using a public key fetched from Azure
Protected Audience KMS. The output is written to stdout with the following
format:
{
    "key_id": <Non negative integer key ID>,
    "public_key_base64": "<Base64-encoded public key>",
    "ciphertext_base64": "<Base64-encoded ciphertext>"
}

With --decrypt_mode, it decrypts the stdin input and output the plaintext using
the private key fetched from Azure Protected Audience KMS. The above json format
is used for the input.
*/

ABSL_FLAG(std::string, public_key_endpoint,
          "https://127.0.0.1:8000/app/listpubkeys",
          "Endpoint serving set of public keys used for encryption");

ABSL_FLAG(std::string, primary_coordinator_private_key_endpoint,
          "https://127.0.0.1:8000/app/key?fmt=tink",
          "Primary coordinator's private key vending service endpoint");

ABSL_FLAG(std::string, unwrap_private_key_endpoint,
          "http://127.0.0.1:8000/app/unwrapKey?fmt=tink",
          "Endpoint to unwrap private key");

ABSL_FLAG(std::string, output_path, "encrypt_payload_out",
          "Path to the output of this tool");

ABSL_FLAG(bool, decrypt_mode, false,
          "If it's true it decrypts the input with the private key fetched "
          "from Azure Protected Audience KMS");

ABSL_FLAG(std::string, get_token_url, "get_token_url",
          "http://127.0.0.1:8000/metadata/identity/oauth2/"
          "token?api-version=2018-02-01");

namespace privacy_sandbox::azure_encrypt_payload {
namespace {
// 45 days. Value for private_key_cache_ttl_seconds in
// services/common/constants/common_service_flags.cc
static constexpr unsigned int kDefaultPrivateKeyCacheTtlSeconds = 3888000;
// 3 hours. Value for key_refresh_flow_run_frequency_seconds in
// services/common/constants/common_service_flags.cc
static constexpr unsigned int kDefaultKeyRefreshFlowRunFrequencySeconds = 10800;

using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::Base64Encode;
using ::google::scp::cpio::Cpio;
using ::google::scp::cpio::CpioOptions;
using ::google::scp::cpio::LogOption;
using PlatformToPublicKeyServiceEndpointMap = absl::flat_hash_map<
    server_common::CloudPlatform,
    std::vector<google::scp::cpio::PublicKeyVendingServiceEndpoint>>;

const quiche::ObliviousHttpHeaderKeyConfig GetOhttpKeyConfig(uint8_t key_id,
                                                             uint16_t kem_id,
                                                             uint16_t kdf_id,
                                                             uint16_t aead_id) {
  const auto ohttp_key_config = quiche::ObliviousHttpHeaderKeyConfig::Create(
      key_id, kem_id, kdf_id, aead_id);
  CHECK(ohttp_key_config.ok()) << "OHTTP key config is not OK";
  return std::move(ohttp_key_config.value());
}

// Copied from data-plane-shared-libraries/src/cpp/communication/ohttp_utils.cc.
// We copied it rather than making it visible to other classes to
// minimize the changes in the existing codes.
absl::StatusOr<uint8_t> ToIntKeyId(absl::string_view key_id) {
  uint32_t val;
  if (!absl::SimpleAtoi(key_id, &val) ||
      val > std::numeric_limits<uint8_t>::max()) {
    return absl::InternalError(
        absl::StrCat("Cannot parse OHTTP key ID from: ", key_id));
  }

  return val;
}

// Based on services/common/encryption/key_fetcher_factory.cc
std::unique_ptr<server_common::PublicKeyFetcherInterface>
CreatePublicKeyFetcher() {
  std::vector<std::string> endpoints = {
      absl::GetFlag(FLAGS_public_key_endpoint)};

  server_common::CloudPlatform cloud_platform =
      server_common::CloudPlatform::kAzure;

  PlatformToPublicKeyServiceEndpointMap per_platform_endpoints = {
      {cloud_platform, endpoints}};
  return server_common::PublicKeyFetcherFactory::Create(per_platform_endpoints);
}

// Based on services/common/encryption/key_fetcher_factory.cc
std::unique_ptr<server_common::KeyFetcherManagerInterface>
CreateKeyFetcherManager(
    std::unique_ptr<server_common::PublicKeyFetcherInterface>
        public_key_fetcher,
    const unsigned int private_key_cache_ttl_seconds =
        kDefaultPrivateKeyCacheTtlSeconds,
    const unsigned int key_refresh_flow_run_frequency_seconds =
        kDefaultKeyRefreshFlowRunFrequencySeconds) {
  google::scp::cpio::PrivateKeyVendingEndpoint primary;

  primary.private_key_vending_service_endpoint =
      absl::GetFlag(FLAGS_primary_coordinator_private_key_endpoint);
  absl::Duration private_key_ttl = absl::Seconds(private_key_cache_ttl_seconds);
  std::unique_ptr<server_common::PrivateKeyFetcherInterface>
      private_key_fetcher = server_common::PrivateKeyFetcherFactory::Create(
          primary, {}, private_key_ttl);

  absl::Duration key_refresh_flow_run_freq =
      absl::Seconds(key_refresh_flow_run_frequency_seconds);

  auto event_engine = std::make_unique<server_common::EventEngineExecutor>(
      grpc_event_engine::experimental::GetDefaultEventEngine());
  std::unique_ptr<server_common::KeyFetcherManagerInterface> manager =
      server_common::KeyFetcherManagerFactory::Create(
          key_refresh_flow_run_freq, std::move(public_key_fetcher),
          std::move(private_key_fetcher), std::move(event_engine));
  manager->Start();

  return manager;
}

struct EncryptPayloadResult {
  std::string ciphertext_base64;
  uint8_t key_id;
  std::string public_key_base64;

  operator nlohmann::json() const {
    return nlohmann::json{{"ciphertext_base64", ciphertext_base64},
                          {"key_id", key_id},
                          {"public_key_base64", public_key_base64}};
  }
};

void from_json(const nlohmann::json& j, EncryptPayloadResult& r) {
  j.at("ciphertext_base64").get_to(r.ciphertext_base64);
  j.at("key_id").get_to(r.key_id);
  j.at("public_key_base64").get_to(r.public_key_base64);
}

EncryptPayloadResult EncryptPayload(
    std::unique_ptr<server_common::KeyFetcherManagerInterface>
        key_fetcher_manager,
    const std::string plaintext_payload) {
  server_common::CloudPlatform cloud_platform =
      server_common::CloudPlatform::kAzure;
  auto public_key = key_fetcher_manager->GetPublicKey(cloud_platform);
  CHECK(public_key.ok()) << "Failed to fetch public key";

  const absl::StatusOr<uint8_t> key_id =
      ToIntKeyId(public_key.value().key_id());
  CHECK(key_id.ok()) << "Failed to get key ID";

  const auto config =
      GetOhttpKeyConfig(key_id.value(), EVP_HPKE_DHKEM_X25519_HKDF_SHA256,
                        EVP_HPKE_HKDF_SHA256, EVP_HPKE_AES_256_GCM);

  std::string decoded_public_key;
  Base64Decode(public_key.value().public_key(), decoded_public_key);

  const auto label_for_test = server_common::kBiddingAuctionOhttpResponseLabel;
  const auto request =
      quiche::ObliviousHttpRequest::CreateClientObliviousRequest(
          plaintext_payload, decoded_public_key, config, label_for_test);
  const std::string ciphertext_bytes = request->EncapsulateAndSerialize();
  std::string ciphertext_base64;
  CHECK(privacy_sandbox::azure_encrypt_payload::Base64Encode(ciphertext_bytes,
                                                             ciphertext_base64)
            .Successful())
      << "Failed to encode ciphertext as base64";

  return {ciphertext_base64, key_id.value(), public_key.value().public_key()};
}

std::string DecryptCiphertext(
    std::unique_ptr<server_common::KeyFetcherManagerInterface>
        key_fetcher_manager,
    const EncryptPayloadResult& encrypt_payload_result) {
  auto key_id = encrypt_payload_result.key_id;

  const auto label_for_test = server_common::kBiddingAuctionOhttpResponseLabel;
  std::optional<server_common::PrivateKey> private_key =
      key_fetcher_manager->GetPrivateKey(std::to_string(key_id));
  CHECK(private_key.has_value()) << "Failed to fetch private key";

  std::string ciphertext_bytes;
  CHECK(privacy_sandbox::azure_encrypt_payload::Base64Decode(
            encrypt_payload_result.ciphertext_base64, ciphertext_bytes)
            .Successful())
      << "Failed to encode ciphertext as base64";

  server_common::EncapsulatedRequest encapsulatedRequest = {ciphertext_bytes,
                                                            label_for_test};

  absl::StatusOr<quiche::ObliviousHttpRequest> decrypted_result =
      server_common::DecryptEncapsulatedRequest(private_key.value(),
                                                encapsulatedRequest);
  CHECK(decrypted_result.ok()) << "Decryption failed";
  return std::string(decrypted_result->GetPlaintextData());
}
}  // namespace
}  // namespace privacy_sandbox::azure_encrypt_payload

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  // Setup
  const auto get_token_url = absl::GetFlag(FLAGS_get_token_url);
  setenv("AZURE_BA_PARAM_GET_TOKEN_URL", get_token_url.c_str(), 0);

  const auto unwrap_private_key_endpoint =
      absl::GetFlag(FLAGS_unwrap_private_key_endpoint);
  setenv("AZURE_BA_PARAM_KMS_UNWRAP_URL", unwrap_private_key_endpoint.c_str(),
         0);

  // grpc_init initializes and shutdown gRPC client.
  privacy_sandbox::server_common::GrpcInit grpc_init;
  google::scp::cpio::CpioOptions cpio_options;
  cpio_options.log_option = google::scp::cpio::LogOption::kConsoleLog;
  CHECK(google::scp::cpio::Cpio::InitCpio(cpio_options).Successful())
      << "Failed to initialize CPIO library";

  auto key_fetcher_manager =
      privacy_sandbox::azure_encrypt_payload::CreateKeyFetcherManager(
          privacy_sandbox::azure_encrypt_payload::CreatePublicKeyFetcher());

  bool decrypt_mode = {absl::GetFlag(FLAGS_decrypt_mode)};

  const std::filesystem::path output_path = absl::GetFlag(FLAGS_output_path);
  if (output_path.has_parent_path()) {
    create_directories(output_path.parent_path());
  }
  std::ofstream fout(output_path);

  if (!decrypt_mode) {
    std::string plaintext_payload;
    std::cin >> plaintext_payload;
    const auto encrypt_result =
        privacy_sandbox::azure_encrypt_payload::EncryptPayload(
            std::move(key_fetcher_manager), plaintext_payload);
    fout << nlohmann::json(encrypt_result).dump(2);

  } else {
    nlohmann::json input;
    std::cin >> input;
    auto encryption_result = input.template get<
        privacy_sandbox::azure_encrypt_payload::EncryptPayloadResult>();
    const auto output_path = absl::GetFlag(FLAGS_output_path);
    fout << DecryptCiphertext(std::move(key_fetcher_manager),
                              encryption_result);
  }

  google::scp::cpio::Cpio::ShutdownCpio(cpio_options);
  return 0;
}
