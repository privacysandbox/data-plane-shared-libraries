/*
 * Copyright 2023 Google LLC
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

#ifndef ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_H_
#define ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "src/encryption//key_fetcher/interface/public_key_fetcher_interface.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/public/cpio/interface/public_key_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"

namespace privacy_sandbox::server_common {

// Implementation of PublicKeyFetcherInterface that fetches the latest set of
// (5) public keys from the Public Key Service and caches them in memory.
class PublicKeyFetcher final : public PublicKeyFetcherInterface {
 public:
  // Initializes an instance of PublicKeyFetcher.
  PublicKeyFetcher(
      absl::flat_hash_map<
          CloudPlatform,
          std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
          public_key_clients,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

  ~PublicKeyFetcher() override = default;

  // Blocking.
  // Calls the Public Key Service to refresh the fetcher's list of the latest
  // list of public keys. Five keys (which are to be used for at most 7 days)
  // are returned and locally cached.
  absl::Status Refresh() noexcept override;

  // Fetches a random public key (from the list of five) for encrypting outgoing
  // requests.
  absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey> GetKey(
      CloudPlatform cloud_platform) noexcept override;

  // Returns the IDs of the cached public keys. Used mainly for unit tests.
  std::vector<google::scp::cpio::PublicPrivateKeyPairId> GetKeyIds(
      CloudPlatform cloud_platform) noexcept override;

 private:
  // PublicKeyClient for fetching public keys from the Public Key Service.
  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients_;

  absl::Mutex mutex_;

  // List of the latest public keys fetched from the Public Key Service.
  absl::flat_hash_map<
      CloudPlatform,
      std::vector<google::cmrt::sdk::public_key_service::v1::PublicKey>>
      public_keys_ ABSL_GUARDED_BY(mutex_);

  // BitGen for randomly choosing a public key to return in GetKey().
  absl::BitGen bitgen_;

  // Log context for PS_VLOG and PS_LOG to enable console or otel logging
  privacy_sandbox::server_common::log::PSLogContext& log_context_;
};

}  // namespace privacy_sandbox::server_common

#endif  // ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_H_
