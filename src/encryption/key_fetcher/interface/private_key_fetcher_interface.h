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

#ifndef CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_PRIVATE_KEY_FETCHER_INTERFACE_H_
#define CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_PRIVATE_KEY_FETCHER_INTERFACE_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "src/logger/request_context_logger.h"
#include "src/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"

namespace privacy_sandbox::server_common {

using PrivateKeyValue = std::string;

// Represents a private key fetched from the Private Key Service.
struct PrivateKey {
  // The ID of the private key. Incoming server requests will have a
  // corresponding public key ID representing the public key that encrypted the
  // ciphertext in the request.
  google::scp::cpio::PublicPrivateKeyPairId key_id;
  // The value of the private key. This field is the raw, unencoded byte string
  // for the private key. It will be passed directly to OHTTP.
  PrivateKeyValue private_key;
  // Creation timestamp of the key. Used by the PrivateKeyFetcher to clear
  // out keys that have been cached for longer than a certain duration.
  absl::Time creation_time;
};

// Interface responsible for fetching and caching private keys.
class PrivateKeyFetcherInterface {
 public:
  virtual ~PrivateKeyFetcherInterface() = default;

  // Fetches and store the private keys for the key IDs passed into the method.
  virtual absl::Status Refresh() noexcept = 0;

  // Returns the corresponding PrivateKey, if present.
  virtual std::optional<PrivateKey> GetKey(
      const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept = 0;
};

// Factory to create PrivateKeyFetcher.
class PrivateKeyFetcherFactory {
 public:
  // Creates a PrivateKeyFetcher given the necessary config and a
  // TTL of when cached keys should be removed from the cache.
  static std::unique_ptr<PrivateKeyFetcherInterface> Create(
      const google::scp::cpio::PrivateKeyVendingEndpoint& primary_endpoint,
      const std::vector<google::scp::cpio::PrivateKeyVendingEndpoint>&
          secondary_endpoints,
      absl::Duration key_ttl,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace privacy_sandbox::server_common

#endif  // CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_PRIVATE_KEY_FETCHER_INTERFACE_H_
