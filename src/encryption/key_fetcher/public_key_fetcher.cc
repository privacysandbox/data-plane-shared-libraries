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

#include "src/encryption/key_fetcher/public_key_fetcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/random/distributions.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/mutex.h"
#include "src/core/interface/errors.h"
#include "src/encryption/key_fetcher/key_fetcher_utils.h"
#include "src/metric/key_fetch.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "src/public/cpio/interface/public_key_client/type_def.h"

namespace privacy_sandbox::server_common {

using ::google::scp::core::ExecutionResult;
using ::google::scp::core::FailureExecutionResult;
using ::google::scp::core::SuccessExecutionResult;
using ::google::scp::core::errors::GetErrorMessage;

using ::google::cmrt::sdk::public_key_service::v1::ListPublicKeysRequest;
using ::google::cmrt::sdk::public_key_service::v1::ListPublicKeysResponse;
using ::google::cmrt::sdk::public_key_service::v1::PublicKey;
using ::google::protobuf::util::TimeUtil;
using ::google::scp::cpio::PublicKeyClientInterface;
using ::google::scp::cpio::PublicKeyClientOptions;
using ::google::scp::cpio::PublicPrivateKeyPairId;
using ::google::scp::cpio::Timestamp;

namespace {
constexpr std::string_view kKeyFetchFailMessage =
    "ListPublicKeys call failed (status_code: $0)";
constexpr std::string_view kKeyFetchSuccessMessage =
    "Successfully fetched latest public keys: (key IDs: [$0], expiration time: "
    "$1)";
}  // namespace

PublicKeyFetcher::PublicKeyFetcher(
    absl::flat_hash_map<
        CloudPlatform,
        std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
        public_key_clients,
    privacy_sandbox::server_common::log::PSLogContext& log_context)
    : public_key_clients_(std::move(public_key_clients)),
      log_context_(log_context) {}

/**
 * Makes a blocking call to fetch the public keys using public key clients
 * for each provided platform.
 *
 * Errors will be logged but an OkStatus will always be returned after
 * attempting to fetch keys from each platform.
 */
absl::Status PublicKeyFetcher::Refresh() noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
  PS_VLOG(3, log_context_) << "Refreshing public keys...";
  absl::BlockingCounter all_fetches_done(public_key_clients_.size());

  for (const auto& [cloud_platform, public_key_client] : public_key_clients_) {
    if (const absl::Status error = public_key_client->ListPublicKeys(
            ListPublicKeysRequest(),
            [this, &all_fetches_done, platform = cloud_platform](
                ExecutionResult execution_result,
                ListPublicKeysResponse response) {
              PS_VLOG(3, log_context_) << "List public keys call finished.";
              if (execution_result.Successful()) {
                int num_public_keys_parsed = 0;
                // TODO(b/395703311): Clear expired public keys from cache.
                if (!response.public_keys().empty()) {
                  std::vector<PublicKey> platform_public_keys;
                  platform_public_keys.reserve(response.public_keys().size());
                  for (const auto& key : response.public_keys()) {
                    PublicKey copy;
                    absl::StatusOr<std::string> ohttp_key_id =
                        ToOhttpKeyId(key.key_id());
                    if (!ohttp_key_id.ok()) {
                      PS_LOG(ERROR, log_context_)
                          << "Error during public key fetch: "
                          << ohttp_key_id.status();
                      continue;
                    }

                    copy.set_key_id(*std::move(ohttp_key_id));
                    copy.set_public_key(key.public_key());
                    platform_public_keys.push_back(std::move(copy));
                    num_public_keys_parsed++;
                  }

                  {
                    absl::MutexLock lock(&mutex_);
                    public_keys_[platform] = std::move(platform_public_keys);
                  }
                }

                KeyFetchResultCounter::SetNumPublicKeysParsed(
                    platform, num_public_keys_parsed);

                {
                  absl::MutexLock lock(&mutex_);
                  KeyFetchResultCounter::SetNumPublicKeysCached(
                      platform, public_keys_.contains(platform)
                                    ? public_keys_[platform].size()
                                    : 0);
                }

                PS_VLOG(3, log_context_) << absl::Substitute(
                    kKeyFetchSuccessMessage,
                    absl::StrJoin(GetKeyIds(platform), ", "),
                    TimeUtil::ToString(response.expiration_time()));
                PS_VLOG(3, log_context_)
                    << "Public key refresh flow completed successfully. ";
              } else {
                KeyFetchResultCounter::
                    IncrementPublicKeyFetchAsyncFailureCount();
                KeyFetchResultCounter::SetNumPublicKeysParsed(platform, 0);
                {
                  absl::MutexLock lock(&mutex_);
                  KeyFetchResultCounter::SetNumPublicKeysCached(
                      platform, public_keys_[platform].size());
                }
                PS_LOG(ERROR, log_context_) << absl::Substitute(
                    kKeyFetchFailMessage,
                    GetErrorMessage(execution_result.status_code));
              }

              all_fetches_done.DecrementCount();
            });
        !error.ok()) {
      PS_LOG(ERROR, log_context_)
          << absl::Substitute(kKeyFetchFailMessage, error.message());
      all_fetches_done.DecrementCount();
    }
  }

  all_fetches_done.Wait();
  return absl::OkStatus();
}

absl::StatusOr<PublicKey> PublicKeyFetcher::GetKey(
    CloudPlatform cloud_platform) noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock lock(&mutex_);
  if (public_keys_.empty()) {
    return absl::FailedPreconditionError(
        "No public keys cached for any cloud platforms");
  }

  auto it = public_keys_.find(cloud_platform);
  if (it == public_keys_.end() || it->second.empty()) {
    return absl::FailedPreconditionError(
        absl::StrCat("No public keys to return for cloud platform ",
                     CloudPlatformEnumToString(cloud_platform)));
  }

  int index = absl::Uniform(absl::IntervalClosedOpen, bitgen_, 0,
                            static_cast<int>(it->second.size()));
  return it->second.at(index);
}

std::vector<PublicPrivateKeyPairId> PublicKeyFetcher::GetKeyIds(
    CloudPlatform cloud_platform) noexcept ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock lock(&mutex_);
  std::vector<PublicPrivateKeyPairId> key_pair_ids;
  for (const auto& key : public_keys_[cloud_platform]) {
    key_pair_ids.push_back(std::string(key.key_id()));
  }

  return key_pair_ids;
}

std::unique_ptr<PublicKeyFetcherInterface> PublicKeyFetcherFactory::Create(
    const absl::flat_hash_map<
        CloudPlatform,
        std::vector<google::scp::cpio::PublicKeyVendingServiceEndpoint>>&
        per_platform_endpoints,
    privacy_sandbox::server_common::log::PSLogContext& log_context) {
  absl::flat_hash_map<
      CloudPlatform,
      std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>>
      public_key_clients;

  for (const auto& [cloud_platform, endpoints] : per_platform_endpoints) {
    PublicKeyClientOptions options;
    options.endpoints = endpoints;

    std::unique_ptr<PublicKeyClientInterface> public_key_client =
        google::scp::cpio::PublicKeyClientFactory::Create(std::move(options));
    public_key_clients[cloud_platform] = std::move(public_key_client);
  }

  return std::make_unique<PublicKeyFetcher>(std::move(public_key_clients),
                                            log_context);
}

}  // namespace privacy_sandbox::server_common
