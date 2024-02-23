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

#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "src/concurrent/executor.h"
#include "src/encryption/key_fetcher/interface/key_fetcher_manager_interface.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/interface/public_key_fetcher_interface.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/public/cpio/interface/type_def.h"

namespace privacy_sandbox::server_common {

// Implementation of KeyFetcherManagerInterface that runs a background task that
// will periodically wake to fetch and maintain the latest public and private
// keys through its public and private key fetchers.
class KeyFetcherManager : public KeyFetcherManagerInterface {
 public:
  KeyFetcherManager(
      absl::Duration key_refresh_period,
      std::unique_ptr<privacy_sandbox::server_common::PublicKeyFetcherInterface>
          public_key_fetcher,
      std::unique_ptr<
          privacy_sandbox::server_common::PrivateKeyFetcherInterface>
          private_key_fetcher,
      std::shared_ptr<privacy_sandbox::server_common::Executor> executor);

  // Waits for any in-flight key fetch flows to complete, cancels the next
  // queued key fetch flow run, and cleans up the key service clients.
  ~KeyFetcherManager();

  // Begins the (endlessly running) background thread that periodically wakes to
  // trigger the key fetchers to refresh their key caches.
  void Start() noexcept override;

  // Fetches a public key used for encrypting outgoing requests.
  absl::StatusOr<google::cmrt::sdk::public_key_service::v1::PublicKey>
  GetPublicKey(CloudPlatform cloud_platform) noexcept override;

  // Fetches the corresponding private key for a given key ID.
  std::optional<privacy_sandbox::server_common::PrivateKey> GetPrivateKey(
      const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept
      override;

 private:
  void RunPeriodicKeyRefresh();

  // How often to wake the background thread to run the key refresh logic.
  absl::Duration key_refresh_period_;

  std::shared_ptr<privacy_sandbox::server_common::Executor> executor_;

  std::unique_ptr<privacy_sandbox::server_common::PublicKeyFetcherInterface>
      public_key_fetcher_;
  std::unique_ptr<privacy_sandbox::server_common::PrivateKeyFetcherInterface>
      private_key_fetcher_;

  // Synchronizes the status of shutdown for destructor and execution loop.
  absl::Notification shutdown_requested_;

  // Identifier for the next, queued up key refresh task on the executor.
  TaskId task_id_;
};

}  // namespace privacy_sandbox::server_common

#endif  // ENCRYPTION_KEY_FETCHER_KEY_FETCHER_MANAGER_H_
