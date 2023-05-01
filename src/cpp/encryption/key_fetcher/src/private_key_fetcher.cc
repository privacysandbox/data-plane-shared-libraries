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

#include "src/cpp/encryption/key_fetcher/src/private_key_fetcher.h"

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cc/core/interface/errors.h"
#include "cc/core/interface/type_def.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "cc/public/cpio/interface/private_key_client/type_def.h"
#include "glog/logging.h"

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

static constexpr absl::string_view kKeyFetchFailMessage =
    "GetEncryptedPrivateKey call failed (key IDs: %s, status_code: %s)";

void HandleFailure(
    const google::protobuf::RepeatedPtrField<std::string>& key_ids,
    google::scp::core::StatusCode status_code) noexcept {
  std::string key_ids_str = absl::StrJoin(key_ids, ", ");
  const std::string error = absl::StrFormat(kKeyFetchFailMessage, key_ids_str,
                                            GetErrorMessage(status_code));
  VLOG(-1) << error;
}

absl::Time ProtoToAbslDuration(const google::protobuf::Timestamp& timestamp) {
  return absl::FromUnixSeconds(timestamp.seconds()) +
         absl::Nanoseconds(timestamp.nanos());
}

}  // namespace

PrivateKeyFetcher::PrivateKeyFetcher(
    std::unique_ptr<google::scp::cpio::PrivateKeyClientInterface>
        private_key_client,
    absl::Duration ttl)
    : private_key_client_(std::move(private_key_client)), ttl_(ttl) {}

PrivateKeyFetcher::~PrivateKeyFetcher() {
  ExecutionResult result = private_key_client_->Stop();
  VLOG_IF(-1, !result.Successful()) << GetErrorMessage(result.status_code);
}

void PrivateKeyFetcher::Refresh(
    const std::vector<PublicPrivateKeyPairId>& key_ids) noexcept
    ABSL_LOCKS_EXCLUDED(mutex_) {
  ListPrivateKeysRequest request;
  request.mutable_key_ids()->Add(key_ids.begin(), key_ids.end());

  ExecutionResult result = private_key_client_.get()->ListPrivateKeys(
      request, [keys_map = &private_keys_map_, mu = &mutex_, request](
                   const ExecutionResult result,
                   const ListPrivateKeysResponse response) {
        if (result.Successful()) {
          absl::MutexLock l(mu);
          for (const auto& private_key : response.private_keys()) {
            PrivateKey key = {private_key.key_id(), private_key.private_key(),
                              ProtoToAbslDuration(private_key.creation_time())};
            keys_map->insert_or_assign(key.key_id, key);
          }
        } else {
          HandleFailure(request.key_ids(), result.status_code);
        }
      });

  if (!result.Successful()) {
    HandleFailure(request.key_ids(), result.status_code);
  }

  absl::MutexLock l(&mutex_);
  // Clean up keys that have been stored in the cache for longer than the ttl.
  absl::Time cutoff_time = absl::Now() - ttl_;
  for (auto it = private_keys_map_.cbegin(); it != private_keys_map_.cend();) {
    if (it->second.creation_time < cutoff_time) {
      private_keys_map_.erase(it++);
    } else {
      it++;
    }
  }
}

std::optional<PrivateKey> PrivateKeyFetcher::GetKey(
    const google::scp::cpio::PublicPrivateKeyPairId& public_key_id) noexcept
    ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock l(&mutex_);
  if (private_keys_map_.find(public_key_id) != private_keys_map_.end()) {
    return std::optional<PrivateKey>(private_keys_map_[public_key_id]);
  }

  return std::nullopt;
}

std::unique_ptr<PrivateKeyFetcherInterface> PrivateKeyFetcherFactory::Create(
    const AccountIdentity& account_identity, const Region& service_region,
    const PrivateKeyVendingServiceEndpoint& primary_coordinator_endpoint,
    const PrivateKeyVendingServiceEndpoint& secondary_coordinator_endpoint,
    absl::Duration key_ttl) {
  PrivateKeyVendingEndpoint primary, secondary;
  primary.account_identity = secondary.account_identity = account_identity;
  primary.service_region = secondary.service_region = service_region;
  primary.private_key_vending_service_endpoint = primary_coordinator_endpoint;
  secondary.private_key_vending_service_endpoint =
      secondary_coordinator_endpoint;

  PrivateKeyClientOptions options;
  options.primary_private_key_vending_endpoint = primary;
  options.secondary_private_key_vending_endpoints = {secondary};

  std::unique_ptr<PrivateKeyClientInterface> private_key_client =
      google::scp::cpio::PrivateKeyClientFactory::Create(options);
  return std::make_unique<PrivateKeyFetcher>(std::move(private_key_client),
                                             key_ttl);
}

}  // namespace privacy_sandbox::server_common
