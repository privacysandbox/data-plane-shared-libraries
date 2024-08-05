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

#ifndef CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_PUBLIC_KEY_FETCHER_INTERFACE_H_
#define CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_PUBLIC_KEY_FETCHER_INTERFACE_H_

#include <memory>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/logger/request_context_logger.h"
#include "src/public/core/interface/cloud_platform.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {

// Interface responsible for fetching and caching public keys.
class PublicKeyFetcherInterface {
 public:
  virtual ~PublicKeyFetcherInterface() = default;

  // Refreshes the fetcher's list of the latest public keys and, upon a
  // successful key fetch, invokes the callback passed into the method.
  virtual absl::Status Refresh() noexcept = 0;

  // Returns a public key for encrypting outgoing requests.
  virtual absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetKey(CloudPlatform cloud_platform) noexcept = 0;

  // Returns the IDs of the cached public keys. For testing purposes only.
  virtual std::vector<google::scp::cpio::PublicPrivateKeyPairId> GetKeyIds(
      CloudPlatform cloud_platform) noexcept = 0;
};

// Factory to create PublicKeyFetcher.
class PublicKeyFetcherFactory {
 public:
  // Creates a PublicKeyFetcher given a list of Public Key Service endpoints.
  static std::unique_ptr<PublicKeyFetcherInterface> Create(
      const absl::flat_hash_map<
          CloudPlatform,
          std::vector<google::scp::cpio::PublicKeyVendingServiceEndpoint>>&
          endpoints,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace privacy_sandbox::server_common

#endif  // CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_PUBLIC_KEY_FETCHER_INTERFACE_H_
