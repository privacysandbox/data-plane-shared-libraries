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

#include <string>
#include <thread>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "src/encryption/key_fetcher/interface/private_key_fetcher_interface.h"
#include "src/encryption/key_fetcher/interface/public_key_fetcher_interface.h"
#include "src/metric/key_fetch.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace privacy_sandbox::server_common {

namespace {

absl::Status MergeStatuses(absl::Status&& status1, absl::Status&& status2) {
  if (!status1.ok() && !status2.ok()) {
    std::string combined_message = absl::StrCat(
        "Multiple errors: ", status1.message(), "; ", status2.message());
    return absl::Status(absl::StatusCode::kInternal, combined_message);
  }

  status1.Update(status2);
  return status1;
}

absl::Status RefreshKeys(PublicKeyFetcherInterface* public_key_fetcher,
                         PrivateKeyFetcherInterface* private_key_fetcher) {
  absl::Status key_fetch_result = absl::OkStatus();

  if (public_key_fetcher) {
    key_fetch_result = public_key_fetcher->Refresh();
    if (!key_fetch_result.ok()) {
      KeyFetchResultCounter::IncrementPublicKeyFetchSyncFailureCount();
      key_fetch_result = absl::Status(
          key_fetch_result.code(), absl::StrCat("Public key refresh failed: ",
                                                key_fetch_result.message()));
    }
  }

  absl::Status private_key_refresh_status = private_key_fetcher->Refresh();
  if (!private_key_refresh_status.ok()) {
    KeyFetchResultCounter::IncrementPrivateKeyFetchSyncFailureCount();
    private_key_refresh_status =
        absl::Status(private_key_refresh_status.code(),
                     absl::StrCat("Private key refresh failed: ",
                                  private_key_refresh_status.message()));
    key_fetch_result = MergeStatuses(std::move(key_fetch_result),
                                     std::move(private_key_refresh_status));
  }

  return key_fetch_result;
}

}  // namespace

using ::google::cmrt::sdk::public_key_service::v1::PublicKey;
using ::google::scp::cpio::PublicPrivateKeyPairId;
using ::privacy_sandbox::server_common::PrivateKeyFetcherInterface;
using ::privacy_sandbox::server_common::PublicKeyFetcherInterface;

// @param key_refresh_period how often the key refresh flow is to be run.
// @public_key_fetcher client for interacting with the Public Key Service
// @private_key_fetcher client for interacting with the Private Key Service
KeyFetcherManager::KeyFetcherManager(
    absl::Duration key_refresh_period,
    std::unique_ptr<PublicKeyFetcherInterface> public_key_fetcher,
    std::unique_ptr<PrivateKeyFetcherInterface> private_key_fetcher,
    privacy_sandbox::server_common::log::PSLogContext& log_context)
    : key_refresh_period_(key_refresh_period),
      public_key_fetcher_(std::move(public_key_fetcher)),
      private_key_fetcher_(std::move(private_key_fetcher)),
      log_context_(log_context),
      key_refresh_closure_(PeriodicClosure::Create()) {}

KeyFetcherManager::~KeyFetcherManager() { key_refresh_closure_->Stop(); }

absl::Status KeyFetcherManager::Start() noexcept {
  absl::Status key_fetch_status =
      RefreshKeys(public_key_fetcher_.get(), private_key_fetcher_.get());
  if (!key_fetch_status.ok()) {
    PS_LOG(ERROR, log_context_)
        << "Initial key fetch failed: " << key_fetch_status;
    return key_fetch_status;
  }

  (void)key_refresh_closure_->StartDelayed(
      key_refresh_period_, [this]() { RunPeriodicKeyRefresh(); });
  return absl::OkStatus();
}

void KeyFetcherManager::RunPeriodicKeyRefresh() {
  absl::Status key_fetch_status =
      RefreshKeys(public_key_fetcher_.get(), private_key_fetcher_.get());
  if (!key_fetch_status.ok()) {
    PS_LOG(ERROR, log_context_) << key_fetch_status;
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
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  return std::make_unique<KeyFetcherManager>(
      key_refresh_period, std::move(public_key_fetcher),
      std::move(private_key_fetcher), log_context);
}

}  // namespace privacy_sandbox::server_common
