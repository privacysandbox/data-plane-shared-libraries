// Copyright 2023 Google LLC
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

#include "src/cpp/encryption/key_fetcher/src/fake_key_fetcher_manager.h"

#include <string>

#include "absl/strings/escaping.h"

namespace privacy_sandbox::server_common {

FakeKeyFetcherManager::FakeKeyFetcherManager(absl::string_view public_key,
                                             absl::string_view private_key) {
  public_key_.set_key_id("5");
  public_key_.set_public_key(
      absl::Base64Escape(absl::HexStringToBytes(public_key)));

  private_key_.key_id = "5";
  private_key_.private_key =
      absl::Base64Escape(absl::HexStringToBytes(private_key));
}

void FakeKeyFetcherManager::Start() noexcept {}

// Fetches a public key used for encrypting outgoing requests.
absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
FakeKeyFetcherManager::GetPublicKey() noexcept {
  return public_key_;
}

// Fetches the corresponding private key for a given key ID.
std::optional<server_common::PrivateKey> FakeKeyFetcherManager::GetPrivateKey(
    const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept {
  return private_key_;
}

}  // namespace privacy_sandbox::server_common
