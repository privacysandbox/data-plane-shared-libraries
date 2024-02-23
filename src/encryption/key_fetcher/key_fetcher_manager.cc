// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/encryption/key_fetcher/key_fetcher_manager.h"

#include <thread>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/interface/public_key_fetcher_interface.h"
#include "src/metric/key_fetch.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {

using ::google::cmrt::sdk::public_key_service::v1::PublicKey;
using ::google::scp::cpio::PublicPrivateKeyPairId;
using ::privacy_sandbox::server_common::PrivateKeyFetcherInterface;
using ::privacy_sandbox::server_common::PublicKeyFetcherInterface;

// @param key_refresh_period how often the key refresh flow is to be run.
// @public_key_fetcher client for interacting with the Public Key Service
// @private_key_fetcher client for interacting with the Private Key Service
// @executor executor on which the key refresh tasks will run.
KeyFetcherManager::KeyFetcherManager(
    absl::Duration key_refresh_period,
    std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher,
    std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher,
    std::shared_ptr<privacy_sandbox::server_common::Executor> executor)
    : key_refresh_period_(key_refresh_period),
      executor_(std::move(executor)),
      public_key_fetcher_(std::move(public_key_fetcher)),
      private_key_fetcher_(std::move(private_key_fetcher)) {}

KeyFetcherManager::~KeyFetcherManager() {
  shutdown_requested_.Notify();
  // Cancel the next queued up key refresh task.
  executor_->Cancel(std::move(task_id_));
}

void KeyFetcherManager::Start() noexcept { RunPeriodicKeyRefresh(); }

void KeyFetcherManager::RunPeriodicKeyRefresh() {
  // Queue up another key refresh task.
  task_id_ = executor_->RunAfter(key_refresh_period_,
                                 [this]() { RunPeriodicKeyRefresh(); });

  if (!shutdown_requested_.HasBeenNotified()) {
    if (public_key_fetcher_) {
      absl::Status public_key_refresh_status = public_key_fetcher_->Refresh();
      if (!public_key_refresh_status.ok()) {
        KeyFetchResultCounter::IncrementPublicKeyFetchSyncFailureCount();
        VLOG(1) << "Public key refresh failed: "
                << public_key_refresh_status.message();
      }
    }

    absl::Status private_key_refresh_status = private_key_fetcher_->Refresh();
    if (!private_key_refresh_status.ok()) {
      KeyFetchResultCounter::IncrementPrivateKeyFetchSyncFailureCount();
      VLOG(1) << "Private key refresh failed: " << private_key_refresh_status;
    }
  } else {
    VLOG(3) << "Shutdown requested; skipping run of KeyFetcherManager's key "
               "refresh flow.";
  }
}

absl::StatusOr<PublicKey> KeyFetcherManager::GetPublicKey(
    CloudPlatform cloud_platform) noexcept {
  return public_key_fetcher_->GetKey(cloud_platform);
}

std::optional<PrivateKey> KeyFetcherManager::GetPrivateKey(
    const google::scp::cpio::PublicPrivateKeyPairId& key_id) noexcept {
  return private_key_fetcher_->GetKey(key_id);
}

std::unique_ptr<KeyFetcherManagerInterface> KeyFetcherManagerFactory::Create(
    absl::Duration key_refresh_period,
    std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher,
    std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher,
    std::shared_ptr<privacy_sandbox::server_common::Executor> executor) {
  return std::make_unique<KeyFetcherManager>(
      key_refresh_period, std::move(public_key_fetcher),
      std::move(private_key_fetcher), std::move(executor));
}

}  // namespace privacy_sandbox::server_common
