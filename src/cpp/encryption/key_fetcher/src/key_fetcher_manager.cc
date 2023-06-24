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

#include "src/cpp/encryption/key_fetcher/src/key_fetcher_manager.h"

#include <thread>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "cc/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "glog/logging.h"
#include "src/cpp/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/cpp/encryption/key_fetcher/interface/public_key_fetcher_interface.h"

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
      public_key_fetcher_(std::move(public_key_fetcher)),
      private_key_fetcher_(std::move(private_key_fetcher)),
      executor_(std::move(executor)) {}

KeyFetcherManager::~KeyFetcherManager() {
  shutdown_requested_.Notify();

  // Stop the key fetchers.
  public_key_fetcher_.reset();
  private_key_fetcher_.reset();

  // Cancel the next queued up key refresh task.
  executor_->Cancel(std::move(task_id_));
}

void KeyFetcherManager::Start() noexcept { RunPeriodicKeyRefresh(); }

// TODO(b/267505670): Add a check such that only one key refresh flow runs at a
//  time.
void KeyFetcherManager::RunPeriodicKeyRefresh() {
  // Queue up another key refresh task.
  task_id_ = executor_->RunAfter(key_refresh_period_,
                                 [this]() { RunPeriodicKeyRefresh(); });

  std::function<void()> pub_key_refresh_callback = [this]() -> void {
    std::vector<PublicPrivateKeyPairId> public_key_ids =
        public_key_fetcher_->GetKeyIds();

    // Filter out any key IDs whose private keys are already cached.
    for (auto iterator = public_key_ids.begin();
         iterator != public_key_ids.end();) {
      if (private_key_fetcher_->GetKey(*iterator).has_value()) {
        iterator = public_key_ids.erase(iterator);
      } else {
        ++iterator;
      }
    }
    VLOG(3) << "Refreshing private keys...";
    private_key_fetcher_->Refresh();
  };

  if (!shutdown_requested_.HasBeenNotified()) {
    absl::Status refresh_status =
        public_key_fetcher_->Refresh(pub_key_refresh_callback);
    if (!refresh_status.ok()) {
      VLOG(1) << "Public key refresh failed: " << refresh_status.message();
    }
  } else {
    VLOG(3) << "Shutdown requested; skipping run of KeyFetcherManager's key "
               "refresh flow.";
  }
}

absl::StatusOr<PublicKey> KeyFetcherManager::GetPublicKey() noexcept {
  return public_key_fetcher_->GetKey();
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
