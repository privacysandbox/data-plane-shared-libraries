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

#ifndef CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_KEY_FETCHER_MANAGER_INTERFACE_H_
#define CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_KEY_FETCHER_MANAGER_INTERFACE_H_

#include <memory>

#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/interface/public_key_fetcher_interface.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/public/cpio/interface/type_def.h"

namespace privacy_sandbox::server_common {

// Interface responsible for returning public/private keys for cryptographic
// purposes.
class KeyFetcherManagerInterface {
 public:
  // Waits for any in-flight key fetch flows to complete and terminates any
  // resources used by the manager.
  virtual ~KeyFetcherManagerInterface() = default;

  // Fetches a public key to be used for encrypting outgoing requests.
  virtual absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetPublicKey(CloudPlatform cloud_platform) noexcept = 0;

  // Fetches the corresponding private key for a public key ID.
  virtual std::optional<PrivateKey> GetPrivateKey(
      const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept = 0;

  // Queues key refresh jobs as often as defined by 'key_refresh_period'. The
  // returned status indicates whether the *initial* key fetching is successful
  // and can be used to determine if a server can accept traffic on startup.
  // TODO: Expose a way to get changes in status of whether the server has the
  // latest encryption keys and should be accepting client traffic.
  virtual absl::Status Start() noexcept = 0;
};

// Factory to create KeyFetcherManager.
class KeyFetcherManagerFactory {
 public:
  static std::unique_ptr<KeyFetcherManagerInterface> Create(
      absl::Duration key_refresh_period,
      std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher,
      std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));
};

}  // namespace privacy_sandbox::server_common

#endif  // CPP_ENCRYPTION_KEY_FETCHER_INTERFACE_KEY_FETCHER_MANAGER_INTERFACE_H_
