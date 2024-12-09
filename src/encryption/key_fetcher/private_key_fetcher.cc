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

#include "src/encryption/key_fetcher/private_key_fetcher.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "proto/hpke.pb.h"
#include "proto/tink.pb.h"
#include "src/core/interface/errors.h"
#include "src/core/interface/type_def.h"
#include "src/encryption/key_fetcher/key_fetcher_utils.h"
#include "src/metric/key_fetch.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using ::google::scp::cpio::PrivateKeyClientInterface;
using ::google::scp::cpio::PrivateKeyClientOptions;

using ::google::scp::core::PublicPrivateKeyPairId;

using ::google::scp::cpio::AccountIdentity;
using ::google::scp::cpio::PrivateKeyVendingEndpoint;
using ::google::scp::cpio::PrivateKeyVendingServiceEndpoint;
using ::google::scp::cpio::Region;

namespace privacy_sandbox::server_common {
namespace {

constexpr std::string_view kKeyFetchFailMessage =
    "GetEncryptedPrivateKey call failed (key IDs: $0, status_code: $1)";

absl::Status HandleFailure(
    const google::protobuf::RepeatedPtrField<std::string>& key_ids,
    google::scp::core::StatusCode status_code,
    privacy_sandbox::server_common::log::PSLogContext& log_context) noexcept {
  std::string key_ids_str = absl::StrJoin(key_ids, ", ");
  const std::string error = absl::Substitute(kKeyFetchFailMessage, key_ids_str,
                                             GetErrorMessage(status_code));
  PS_LOG(ERROR, log_context) << error;
  return absl::UnavailableError(error);
}

absl::Time ProtoToAbslDuration(const google::protobuf::Timestamp& timestamp) {
  return absl::FromUnixSeconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

}  // namespace

PrivateKeyFetcher::PrivateKeyFetcher(
    std::unique_ptr<google::scp::cpio::PrivateKeyClientInterface>
        private_key_client,
    absl::Duration ttl,
    privacy_sandbox::server_common::log::PSLogContext& log_context)
    : private_key_client_(std::move(private_key_client)),
      ttl_(ttl),
      log_context_(log_context) {}

absl::Status PrivateKeyFetcher::Refresh() noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
  PS_VLOG(3, log_context_) << "Refreshing private keys...";

  ListPrivateKeysRequest request;
  request.set_max_age_seconds(ToInt64Seconds(ttl_));

  absl::Notification fetch_notify;
  auto list_priv_key_cb = [request, &fetch_notify, this](
                              const ExecutionResult result,
                              const ListPrivateKeysResponse response) {
    if (result.Successful()) {
      PS_VLOG(3, log_context_) << "Number of private keys listed: "
                               << response.private_keys().size();
      KeyFetchResultCounter::SetNumPrivateKeysFetched(
          response.private_keys().size());
      absl::MutexLock lock(&mutex_);
      int num_priv_keys_added = 0;
      for (const auto& private_key : response.private_keys()) {
        std::string keyset_bytes;
        if (!absl::Base64Unescape(private_key.private_key(), &keyset_bytes)) {
          PS_LOG(ERROR, log_context_)
              << "Could not base 64 decode the keyset. Key Id: "
              << private_key.key_id();
          continue;
        }
        google::crypto::tink::Keyset keyset;
        if (!keyset.ParseFromString(keyset_bytes)) {
          PS_LOG(ERROR, log_context_)
              << "Could not parse a tink::Keyset from the base 64 "
                 "decoded bytes. Key Id: "
              << private_key.key_id();
          continue;
        }
        if (keyset.key().size() != 1) {
          PS_LOG(ERROR, log_context_)
              << "Keyset must contain exactly one key. Key Id: "
              << private_key.key_id();
          continue;
        }
        std::string hpke_priv_key_bytes = keyset.key()[0].key_data().value();
        google::crypto::tink::HpkePrivateKey hpke_priv_key;
        if (!hpke_priv_key.ParseFromString(hpke_priv_key_bytes)) {
          PS_LOG(ERROR, log_context_)
              << "Could not parse the tink::HpkePrivateKey from the "
                 "raw bytes. Key Id: "
              << private_key.key_id();
          continue;
        }
        const PublicPrivateKeyPairId ohttp_key_id =
            ToOhttpKeyId(private_key.key_id());
        PrivateKey key = {
            ohttp_key_id,
            hpke_priv_key.private_key(),
            ProtoToAbslDuration(private_key.creation_time()),
        };
        private_keys_map_.insert_or_assign(ohttp_key_id, std::move(key));
        ++num_priv_keys_added;
        PS_VLOG(2, log_context_) << absl::StrCat(
            "Caching private key: (KMS id: ", private_key.key_id(),
            ", OHTTP ID: ", ohttp_key_id, " )");
      }
      KeyFetchResultCounter::SetNumPrivateKeysParsed(num_priv_keys_added);
    } else {
      KeyFetchResultCounter::IncrementPrivateKeyFetchAsyncFailureCount();
      KeyFetchResultCounter::SetNumPrivateKeysParsed(0);
      HandleFailure(request.key_ids(), result.status_code, log_context_)
          .IgnoreError();
    }
    fetch_notify.Notify();
  };
  if (const absl::Status error = private_key_client_->ListPrivateKeys(
          request, std::move(list_priv_key_cb));
      !error.ok()) {
    auto error_message = absl::Substitute(
        kKeyFetchFailMessage, absl::StrJoin(request.key_ids(), ", "),
        error.message());
    PS_LOG(ERROR, log_context_) << "ListPrivateKeys failed: " << error_message;
    return absl::Status(error.code(), error_message);
  }

  PS_VLOG(3, log_context_) << "Private key fetch pending...";
  fetch_notify.WaitForNotification();

  absl::MutexLock lock(&mutex_);
  // Clean up keys that have been stored in the cache for longer than the ttl.
  absl::Time cutoff_time = absl::Now() - ttl_;
  PS_VLOG(3, log_context_) << "Cleaning up private keys with cutoff time: "
                           << cutoff_time;
  for (auto it = private_keys_map_.cbegin(); it != private_keys_map_.cend();) {
    if (it->second.creation_time < cutoff_time) {
      private_keys_map_.erase(it++);
    } else {
      it++;
    }
  }
  KeyFetchResultCounter::SetNumPrivateKeysCached(private_keys_map_.size());
  return absl::OkStatus();
}

std::optional<PrivateKey> PrivateKeyFetcher::GetKey(
    const google::scp::cpio::PublicPrivateKeyPairId& public_key_id) noexcept
    ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock lock(&mutex_);
  if (const auto it = private_keys_map_.find(public_key_id);
      it != private_keys_map_.end()) {
    return it->second;
  }
  return std::nullopt;
}

std::unique_ptr<PrivateKeyFetcherInterface> PrivateKeyFetcherFactory::Create(
    const PrivateKeyVendingEndpoint& primary_endpoint,
    const std::vector<PrivateKeyVendingEndpoint>& secondary_endpoints,
    absl::Duration key_ttl,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  PrivateKeyClientOptions options;
  options.primary_private_key_vending_endpoint = primary_endpoint;
  options.secondary_private_key_vending_endpoints = secondary_endpoints;
  options.log_context = log_context;

  std::unique_ptr<PrivateKeyClientInterface> private_key_client =
      google::scp::cpio::PrivateKeyClientFactory::Create(options);
  if (!private_key_client->Init().ok()) {
    PS_LOG(ERROR, log_context) << "Failed to initialize private key client.";
  }
  return std::make_unique<PrivateKeyFetcher>(std::move(private_key_client),
                                             key_ttl, log_context);
}

}  // namespace privacy_sandbox::server_common
