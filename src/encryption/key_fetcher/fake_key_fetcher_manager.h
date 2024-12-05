/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ENCRYPTION_KEY_FETCHER_FAKE_KEY_FETCHER_MANAGER_H_
#define ENCRYPTION_KEY_FETCHER_FAKE_KEY_FETCHER_MANAGER_H_

#include <string_view>

#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {

inline constexpr std::string_view kDefaultKeyId = "64";
// Key id 64 public key (only used for testing):
inline constexpr std::string_view kDefaultPublicKeyHex =
    "f3b7b2f1764f5c077effecad2afd86154596e63f7375ea522761b881e6c3c323";
// Key id 64 private key (only used for testing):
inline constexpr std::string_view kDefaultPrivateKeyHex =
    "e7b292f49df28b8065992cdeadbc9d032a0e09e8476cb6d8d507212e7be3b9b4";

// "Fake" implementation of the key fetcher manager that returns hard coded
// keys. Used for testing.
class FakeKeyFetcherManager : public KeyFetcherManagerInterface {
 public:
  // Constructs hard coded keys from the given public/private key material.
  FakeKeyFetcherManager(std::string_view public_key = kDefaultPublicKeyHex,
                        std::string_view private_key = kDefaultPrivateKeyHex,
                        std::string_view key_id = kDefaultKeyId);

  ~FakeKeyFetcherManager() = default;

  // FakeKeyFetcherManager is neither copyable nor movable.
  FakeKeyFetcherManager(const FakeKeyFetcherManager&) = delete;
  FakeKeyFetcherManager& operator=(const FakeKeyFetcherManager&) = delete;

  // No-op.
  absl::Status Start() noexcept override;

  // Fetches hard coded public key.
  absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetPublicKey(CloudPlatform cloud_platform) noexcept override;

  // Fetches private coded public key.
  std::optional<privacy_sandbox::server_common::PrivateKey> GetPrivateKey(
      const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept
      override;

 private:
  google::cmrt::sdk::public_key_service::v1::PublicKey public_key_;
  PrivateKey private_key_;
};

}  // namespace privacy_sandbox::server_common

#endif  // ENCRYPTION_KEY_FETCHER_FAKE_KEY_FETCHER_MANAGER_H_
