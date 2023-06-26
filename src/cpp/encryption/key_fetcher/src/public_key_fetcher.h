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

#ifndef SRC_CPP_ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_H_
#define SRC_CPP_ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/random/random.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "cc/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "cc/public/cpio/interface/public_key_client/type_def.h"
#include "cc/public/cpio/interface/type_def.h"
#include "src/cpp/encryption//key_fetcher/interface/public_key_fetcher_interface.h"

namespace privacy_sandbox::server_common {

// Implementation of PublicKeyFetcherInterface that fetches the latest set of
// (5) public keys from the Public Key Service and caches them in memory.
class PublicKeyFetcher : public PublicKeyFetcherInterface {
 public:
  // Initializes an instance of PublicKeyFetcher.
  PublicKeyFetcher(std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>
                       public_key_client);

  // Stops and terminates any resources used by the fetcher.
  ~PublicKeyFetcher();

  // Blocking.
  // Calls the Public Key Service to refresh the fetcher's list of the latest
  // list of public keys. Five keys (which are to be used for at most 7 days)
  // are returned and locally cached.
  absl::Status Refresh() noexcept override;

  // Fetches a random public key (from the list of five) for encrypting outgoing
  // requests.
  absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetKey() noexcept override;

  // Returns the IDs of the cached public keys. Used mainly for unit tests.
  std::vector<google::scp::cpio::PublicPrivateKeyPairId> GetKeyIds() noexcept
      override;

 private:
  // PublicKeyClient for fetching public keys from the Public Key Service.
  std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>
      public_key_client_;

  absl::Mutex mutex_;

  // List of the latest public keys fetched  from the Public Key Service.
  std::vector<google::cmrt::sdk::public_key_service::v1::PublicKey> public_keys_
      ABSL_GUARDED_BY(mutex_);

  // BitGen for randomly choosing a public key to return in GetKey().
  absl::BitGen bitgen_;
};

}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_ENCRYPTION_KEY_FETCHER_PUBLIC_KEY_FETCHER_H_
