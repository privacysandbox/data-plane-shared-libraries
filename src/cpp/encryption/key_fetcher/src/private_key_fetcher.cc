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

using ::google::scp::cpio::ListPrivateKeysByIdsRequest;
using ::google::scp::cpio::ListPrivateKeysByIdsResponse;
using ::google::scp::cpio::PrivateKeyClientInterface;
using ::google::scp::cpio::PrivateKeyClientOptions;

using ::google::scp::core::PublicPrivateKeyPairId;
using ::google::scp::cpio::PrivateKey;

namespace privacy_sandbox::server_common {
namespace {

static constexpr absl::string_view kKeyFetchFailMessage =
    "GetEncryptedPrivateKey call failed (key IDs: %s, status_code: %s)";

void HandleFailure(const std::vector<PublicPrivateKeyPairId>& key_ids,
                   google::scp::core::StatusCode status_code) noexcept {
  std::string key_ids_str = absl::StrJoin(key_ids, ", ");
  std::string error = absl::StrFormat(kKeyFetchFailMessage, key_ids_str,
                                      GetErrorMessage(status_code));
  VLOG(-1) << error;
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
  ListPrivateKeysByIdsRequest request = {key_ids};
  ExecutionResult result = private_key_client_.get()->ListPrivateKeysByIds(
      request, [keys_map = &private_keys_map_, mu = &mutex_, request](
                   const ExecutionResult result,
                   const ListPrivateKeysByIdsResponse response) {
        if (result.Successful()) {
          absl::MutexLock l(mu);
          for (google::scp::cpio::PrivateKey private_key :
               response.private_keys) {
            PrivateKey key = {private_key.key_id, private_key.private_key,
                              absl::Now()};
            keys_map->insert_or_assign(key.key_id, key);
          }
        } else {
          HandleFailure(request.key_ids, result.status_code);
        }
      });

  if (!result.Successful()) {
    HandleFailure(key_ids, result.status_code);
  }

  absl::MutexLock l(&mutex_);
  // Clean up keys that have been stored in the cache for longer than the ttl.
  absl::Time cutoff_time = absl::Now() - ttl_;
  for (auto it = private_keys_map_.cbegin(); it != private_keys_map_.cend();) {
    if (it->second.key_fetch_time < cutoff_time) {
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

}  // namespace privacy_sandbox::server_common
