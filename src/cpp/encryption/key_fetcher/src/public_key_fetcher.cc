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

#include "src/cpp/encryption/key_fetcher/src/public_key_fetcher.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/random/distributions.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "cc/core/interface/errors.h"
#include "cc/public/core/interface/execution_result.h"
#include "cc/public/cpio/interface/public_key_client/public_key_client_interface.h"
#include "cc/public/cpio/interface/public_key_client/type_def.h"
#include "glog/logging.h"

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

static constexpr absl::string_view kKeyFetchFailMessage =
    "ListPublicKeys call failed (status_code: %s)";
static constexpr absl::string_view kKeyFetchSuccessMessage =
    "Successfully fetched latest public keys: (key IDs: [%s], expiration time: "
    "%s)";

PublicKeyFetcher::PublicKeyFetcher(
    std::unique_ptr<google::scp::cpio::PublicKeyClientInterface>
        public_key_client)
    : public_key_client_(std::move(public_key_client)) {}

PublicKeyFetcher::~PublicKeyFetcher() { public_key_client_->Stop(); }

absl::Status PublicKeyFetcher::Refresh(
    const std::function<void()>& callback) noexcept
    ABSL_LOCKS_EXCLUDED(mutex_) {
  ExecutionResult result = public_key_client_.get()->ListPublicKeys(
      ListPublicKeysRequest(),
      [this, callback = callback](ExecutionResult execution_result,
                                  ListPublicKeysResponse response) {
        if (execution_result.Successful()) {
          mutex_.Lock();
          public_keys_ = std::vector<PublicKey>(response.public_keys().begin(),
                                                response.public_keys().end());
          mutex_.Unlock();

          std::vector<PublicPrivateKeyPairId> key_ids = GetKeyIds();
          std::string key_ids_str = absl::StrJoin(key_ids, ", ");
          VLOG(3) << absl::StrFormat(
              kKeyFetchSuccessMessage, key_ids_str,
              TimeUtil::ToString(response.expiration_time()));
          VLOG(3) << "Public key refresh flow completed successfully. "
                     "Executing callback.";
          std::move(callback)();
        }

        std::string error =
            absl::StrFormat(kKeyFetchFailMessage,
                            GetErrorMessage(execution_result.status_code));
        VLOG_IF(1, !execution_result.Successful()) << error;
      });

  if (!result.Successful()) {
    std::string error = absl::StrFormat(kKeyFetchFailMessage,
                                        GetErrorMessage(result.status_code));
    VLOG(1) << error;
    return absl::UnavailableError(error);
  }

  return absl::OkStatus();
}

absl::StatusOr<PublicKey> PublicKeyFetcher::GetKey() noexcept
    ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock l(&mutex_);
  if (public_keys_.empty()) {
    return absl::FailedPreconditionError("No public keys to return.");
  }

  int index = absl::Uniform(absl::IntervalClosedOpen, bitgen_, 0,
                            static_cast<int>(public_keys_.size()));
  return public_keys_.at(index);
}

std::vector<PublicPrivateKeyPairId> PublicKeyFetcher::GetKeyIds() noexcept
    ABSL_LOCKS_EXCLUDED(mutex_) {
  absl::MutexLock l(&mutex_);
  std::vector<PublicPrivateKeyPairId> key_pair_ids;
  for (const auto& entry : public_keys_) {
    key_pair_ids.push_back(std::string(entry.key_id()));
  }

  return key_pair_ids;
}

std::unique_ptr<PublicKeyFetcherInterface> PublicKeyFetcherFactory::Create(
    const std::vector<google::scp::cpio::PublicKeyVendingServiceEndpoint>&
        endpoints) {
  PublicKeyClientOptions options;
  options.endpoints = endpoints;

  std::unique_ptr<PublicKeyClientInterface> public_key_client =
      google::scp::cpio::PublicKeyClientFactory::Create(std::move(options));

  ExecutionResult init_result = public_key_client->Init();
  if (!init_result.Successful()) {
    VLOG(1) << "Failed to initialize private key client.";
  }

  ExecutionResult run_result = public_key_client->Run();
  if (!run_result.Successful()) {
    VLOG(1) << "Failed to run private key client.";
  }

  return std::make_unique<PublicKeyFetcher>(std::move(public_key_client));
}

}  // namespace privacy_sandbox::server_common
