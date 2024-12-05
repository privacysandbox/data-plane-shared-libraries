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

#ifndef ENCRYPTION_KEY_FETCHER_KEY_FETCHER_MANAGER_H_
#define ENCRYPTION_KEY_FETCHER_KEY_FETCHER_MANAGER_H_

#include <memory>
#include <optional>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/interface/public_key_fetcher_interface.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/util/periodic_closure.h"

namespace privacy_sandbox::server_common {

// Implementation of KeyFetcherManagerInterface that orchestrates fetching and
// managing encryption keys.
class KeyFetcherManager : public KeyFetcherManagerInterface {
 public:
  KeyFetcherManager(
      absl::Duration key_refresh_period,
      std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher,
      std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext));

  // Waits for any in-flight key fetch flows to complete, cancels the next
  // queued key fetch flow run, and cleans up the key service clients.
  ~KeyFetcherManager();

  // Begins the (endlessly running) background thread that periodically wakes to
  // trigger the key fetchers to refresh their key caches.
  absl::Status Start() noexcept override;

  // Fetches a public key used for encrypting outgoing requests.
  absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetPublicKey(CloudPlatform cloud_platform) noexcept override;

  std::optional<privacy_sandbox::server_common::PrivateKey> GetPrivateKey(
      const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept
      override;

 private:
  void RunPeriodicKeyRefresh();

  // How often to refresh encryption keys.
  absl::Duration key_refresh_period_;

  std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher_;
  std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher_;

  // Log context for PS_VLOG and PS_LOG to enable console or otel logging.
  privacy_sandbox::server_common::log::PSLogContext& log_context_;

  std::unique_ptr<PeriodicClosure> key_refresh_closure_;
};

}  // namespace privacy_sandbox::server_common

#endif  // ENCRYPTION_KEY_FETCHER_KEY_FETCHER_MANAGER_H_
