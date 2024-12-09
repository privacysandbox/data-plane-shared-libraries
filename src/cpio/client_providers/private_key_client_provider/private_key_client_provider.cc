/*
 * Copyright 2022 Google LLC
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

#include "private_key_client_provider.h"

#include <atomic>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/http_types.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

#include "error_codes.h"
#include "private_key_client_utils.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::cmrt::sdk::private_key_service::v1::PrivateKey;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpClientInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS;

namespace {
constexpr std::string_view kPrivateKeyClientProvider =
    "PrivateKeyClientProvider";
}

namespace google::scp::cpio::client_providers {
absl::Status PrivateKeyClientProvider::ListPrivateKeys(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context) noexcept {
  PS_VLOG(5, private_key_client_options_.log_context)
      << "Listing private keys...";
  auto list_keys_status = std::make_shared<ListPrivateKeysStatus>();
  if (list_private_keys_context.request->key_ids().empty()) {
    list_keys_status->listing_method = ListingMethod::kByMaxAge;
    PS_VLOG(5, private_key_client_options_.log_context)
        << "Private key listing request by max age.";
  } else {
    list_keys_status->listing_method = ListingMethod::kByKeyId;
    PS_VLOG(5, private_key_client_options_.log_context)
        << "Private key listing request by key ids.";
  }
  list_keys_status->result_list =
      std::vector<KeysResultPerEndpoint>(endpoint_count_);
  PS_VLOG(5, private_key_client_options_.log_context)
      << "Private key listing endpoint count: " << endpoint_count_;

  list_keys_status->call_count_per_endpoint =
      list_keys_status->listing_method == ListingMethod::kByKeyId
          ? list_private_keys_context.request->key_ids().size()
          : 1;
  PS_VLOG(5, private_key_client_options_.log_context)
      << "Private key listing call count per endpoint: "
      << list_keys_status->call_count_per_endpoint;

  for (size_t call_index = 0;
       call_index < list_keys_status->call_count_per_endpoint; ++call_index) {
    for (size_t uri_index = 0; uri_index < endpoint_count_; ++uri_index) {
      auto request = std::make_shared<PrivateKeyFetchingRequest>();

      if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
        request->key_id = std::make_shared<std::string>(
            list_private_keys_context.request->key_ids(call_index));
        PS_VLOG(5, private_key_client_options_.log_context)
            << "Private key fetching request key id: " << request->key_id;
      } else {
        request->max_age_seconds =
            list_private_keys_context.request->max_age_seconds();
        PS_VLOG(5, private_key_client_options_.log_context)
            << "Private key fetching max age seconds: "
            << request->max_age_seconds;
      }

      const auto& endpoint = endpoint_list_[uri_index];
      request->key_vending_endpoint =
          std::make_shared<PrivateKeyVendingEndpoint>(endpoint);

      AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>
          fetch_private_key_context(
              std::move(request),
              std::bind(&PrivateKeyClientProvider::OnFetchPrivateKeyCallback,
                        this, list_private_keys_context, std::placeholders::_1,
                        list_keys_status, uri_index),
              list_private_keys_context);

      auto execution_result =
          private_key_fetcher_->FetchPrivateKey(fetch_private_key_context);

      if (!execution_result.Successful()) {
        // To avoid running context.Finish() repeatedly, use
        // compare_exchange_strong() to check if the ListPrivateKeys
        // context has a result.
        auto got_failure = false;
        if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                  true)) {
          SCP_ERROR(kPrivateKeyClientProvider, kZeroUuid, execution_result,
                    "Failed to fetch private key with endpoint %s.",
                    endpoint.private_key_vending_service_endpoint.c_str());
          list_private_keys_context.Finish(execution_result);
        }
        auto error_message = google::scp::core::errors::GetErrorMessage(
            execution_result.status_code);
        PS_LOG(ERROR, private_key_client_options_.log_context)
            << "Failed to fetch private key with endpoint: "
            << endpoint.private_key_vending_service_endpoint.c_str()
            << ". Error message: " << error_message;
        return absl::UnknownError(error_message);
      }
    }
  }

  return absl::OkStatus();
}

void PrivateKeyClientProvider::OnFetchPrivateKeyCallback(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context,
    AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
        fetch_private_key_context,
    std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
    size_t uri_index) noexcept {
  if (list_keys_status->got_failure.load()) {
    return;
  }

  list_keys_status->fetching_call_returned_count.fetch_add(1);
  auto execution_result = fetch_private_key_context.result;
  if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
    {
      absl::MutexLock lock(&list_keys_status->result_list[uri_index].mu);
      list_keys_status->result_list[uri_index]
          .fetch_result_key_id_map[*fetch_private_key_context.request->key_id] =
          execution_result;
    }
    // For ListByKeyId, store the key IDs no matter the fetching failed or not.
    absl::MutexLock lock(&list_keys_status->set_mutex);
    list_keys_status->key_id_set.insert(
        *fetch_private_key_context.request->key_id);
  } else {
    list_keys_status->result_list[uri_index].fetch_result = execution_result;
  }

  // For empty key list, call callback directly.
  if (!execution_result.Successful() ||
      fetch_private_key_context.response->encryption_keys.empty()) {
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context(
        std::make_shared<DecryptRequest>(), [](auto&) {});
    decrypt_context.result = SuccessExecutionResult();
    OnDecryptCallback(list_private_keys_context, decrypt_context,
                      list_keys_status, nullptr, uri_index);
    return;
  }

  list_keys_status->total_key_split_count.fetch_add(
      fetch_private_key_context.response->encryption_keys.size());

  for (const auto& encryption_key :
       fetch_private_key_context.response->encryption_keys) {
    if (list_keys_status->listing_method == ListingMethod::kByMaxAge) {
      absl::MutexLock lock(&list_keys_status->set_mutex);
      list_keys_status->key_id_set.insert(*encryption_key->key_id);
    }
    DecryptRequest kms_decrypt_request;
    execution_result = PrivateKeyClientUtils::GetKmsDecryptRequest(
        encryption_key, kms_decrypt_request);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          execution_result, "Failed to get the key data.");
        auto error_message = google::scp::core::errors::GetErrorMessage(
            execution_result.status_code);
        PS_LOG(ERROR, private_key_client_options_.log_context)
            << "GetKmsDecryptRequest failed. Error message: " << error_message;
        list_private_keys_context.Finish(execution_result);
      }
      return;
    }
    kms_decrypt_request.set_account_identity(
        fetch_private_key_context.request->key_vending_endpoint
            ->account_identity);
    kms_decrypt_request.set_kms_region(
        fetch_private_key_context.request->key_vending_endpoint
            ->service_region);
    // Only used for GCP.
    kms_decrypt_request.set_gcp_wip_provider(
        fetch_private_key_context.request->key_vending_endpoint
            ->gcp_wip_provider);
    AsyncContext<DecryptRequest, DecryptResponse> decrypt_context(
        std::make_shared<DecryptRequest>(kms_decrypt_request),
        std::bind(&PrivateKeyClientProvider::OnDecryptCallback, this,
                  list_private_keys_context, std::placeholders::_1,
                  list_keys_status, encryption_key, uri_index),
        list_private_keys_context);
    if (absl::Status error = kms_client_provider_->Decrypt(decrypt_context);
        !error.ok()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          error, "Failed to send decrypt request.");
        PS_LOG(ERROR, private_key_client_options_.log_context)
            << "Failed to send decrypt request. Error message: " << error;
        list_private_keys_context.Finish(FailureExecutionResult(SC_UNKNOWN));
      }
      return;
    }
  }
}

namespace {
DecryptResult MakeDecryptResult(EncryptionKey encryption_key,
                                ExecutionResult result, std::string plaintext) {
  DecryptResult decrypt_result;
  decrypt_result.decrypt_result = std::move(result);
  decrypt_result.encryption_key = std::move(encryption_key);
  if (!plaintext.empty()) {
    decrypt_result.plaintext = std::move(plaintext);
  }
  return decrypt_result;
}
}  // namespace

void PrivateKeyClientProvider::OnDecryptCallback(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context,
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
    std::shared_ptr<EncryptionKey> encryption_key, size_t uri_index) noexcept {
  if (list_keys_status->got_failure.load()) {
    return;
  }

  std::atomic<size_t> finished_key_split_count_prev(
      list_keys_status->finished_key_split_count.load() - 1);
  if (encryption_key) {
    std::string plaintext;
    if (decrypt_context.result.Successful()) {
      plaintext = std::move(*decrypt_context.response->mutable_plaintext());
    }
    {
      DecryptResult decrypt_result =
          MakeDecryptResult(*encryption_key, std::move(decrypt_context.result),
                            std::move(plaintext));
      absl::MutexLock lock(&list_keys_status->result_list[uri_index].mu);
      list_keys_status->result_list[uri_index]
          .decrypt_result_key_id_map[*decrypt_result.encryption_key.key_id] =
          std::move(decrypt_result);
    }
    finished_key_split_count_prev =
        list_keys_status->finished_key_split_count.fetch_add(1);
  }

  // Finished all remote calls.
  if (list_keys_status->fetching_call_returned_count ==
          list_keys_status->call_count_per_endpoint * endpoint_count_ &&
      finished_key_split_count_prev ==
          list_keys_status->total_key_split_count - 1) {
    list_private_keys_context.response =
        std::make_shared<ListPrivateKeysResponse>();

    for (auto& key_id : list_keys_status->key_id_set) {
      bool all_splits_are_available = true;
      auto single_party_key = PrivateKeyClientUtils::ExtractSinglePartyKey(
          list_keys_status->result_list, key_id);
      std::vector<DecryptResult> success_decrypt_result;
      if (single_party_key.has_value()) {
        // If contains single party key, ignore the fetch and decrypt results.
        success_decrypt_result.push_back(std::move(single_party_key.value()));
      } else {
        // If doesn't contain single party key, validate every fetch and
        // decrypt results.
        auto execution_result = PrivateKeyClientUtils::ExtractAnyFailure(
            list_keys_status->result_list, key_id);
        if (!execution_result.Successful()) {
          SCP_ERROR_CONTEXT(kPrivateKeyClientProvider,
                            list_private_keys_context, execution_result,
                            "Failed to fetch the private key for key ID: %s",
                            key_id.c_str());
          auto error_message = google::scp::core::errors::GetErrorMessage(
              execution_result.status_code);
          PS_LOG(ERROR, private_key_client_options_.log_context)
              << "Failed to fetch the private key for key ID: "
              << key_id.c_str() << ". Error message: " << error_message;
          list_private_keys_context.Finish(execution_result);
          return;
        }
        // Key splits returned from each endpoint should match the endpoint
        // count.
        success_decrypt_result.reserve(endpoint_count_);
        for (int i = 0; i < endpoint_count_; ++i) {
          std::optional<DecryptResult> decrypt_result;
          {
            auto& result = list_keys_status->result_list[i];
            absl::MutexLock lock(&result.mu);
            if (auto it = result.decrypt_result_key_id_map.find(key_id);
                it != result.decrypt_result_key_id_map.end()) {
              decrypt_result = it->second;
            }
          }
          if (!decrypt_result.has_value() ||
              decrypt_result->encryption_key.key_data.size() !=
                  endpoint_count_) {
            if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
              list_private_keys_context.Finish(FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS));
              SCP_ERROR_CONTEXT(
                  kPrivateKeyClientProvider, list_private_keys_context,
                  list_private_keys_context.result,
                  "Unmatched endpoints number and private key  split "
                  "data size for key ID %s.",
                  encryption_key->key_id->c_str());
              auto error_message = google::scp::core::errors::GetErrorMessage(
                  list_private_keys_context.result.status_code);
              PS_LOG(ERROR, private_key_client_options_.log_context)
                  << "Unmatched endpoints number and private key  split  data "
                     "size for key ID "
                  << encryption_key->key_id->c_str()
                  << ". Error message: " << error_message;
              return;
            } else {
              // For ListByAge, the key split count might not match the
              // endpoint count if the key is corrupted. We just log it
              // instead of error out.
              SCP_WARNING_CONTEXT(
                  kPrivateKeyClientProvider, list_private_keys_context,
                  "Unmatched endpoints number and private key split "
                  "data size for key ID %s.",
                  encryption_key->key_id->c_str());
              PS_VLOG(3, private_key_client_options_.log_context)
                  << "Unmatched endpoints number and private key split data "
                     "size for key ID: "
                  << encryption_key->key_id->c_str();
              all_splits_are_available = false;
              break;
            }
          }
          success_decrypt_result.push_back(*std::move(decrypt_result));
        }
      }
      if (all_splits_are_available) {
        auto private_key_or =
            PrivateKeyClientUtils::ConstructPrivateKey(success_decrypt_result);
        if (!private_key_or.Successful()) {
          SCP_ERROR_CONTEXT(kPrivateKeyClientProvider,
                            list_private_keys_context, private_key_or.result(),
                            "Failed to construct private key.");
          auto error_message = google::scp::core::errors::GetErrorMessage(
              private_key_or.result().status_code);
          PS_LOG(ERROR, private_key_client_options_.log_context)
              << "Failed to construct private key. Error message: "
              << error_message;
          list_private_keys_context.Finish(private_key_or.result());
          return;
        }
        *list_private_keys_context.response->add_private_keys() =
            std::move(*private_key_or);
      }
    }

    list_private_keys_context.Finish(SuccessExecutionResult());
  }
}

absl::Nonnull<std::unique_ptr<PrivateKeyClientProviderInterface>>
PrivateKeyClientProviderFactory::Create(
    PrivateKeyClientOptions options,
    absl::Nonnull<core::HttpClientInterface*> http_client,
    absl::Nonnull<RoleCredentialsProviderInterface*> role_credentials_provider,
    absl::Nonnull<AuthTokenProviderInterface*> auth_token_provider,
    absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) {
  return std::make_unique<PrivateKeyClientProvider>(
      std::move(options),
      PrivateKeyFetcherProviderFactory::Create(
          http_client, role_credentials_provider, auth_token_provider,
          options.log_context),
      KmsClientProviderFactory::Create(role_credentials_provider,
                                       io_async_executor));
}

}  // namespace google::scp::cpio::client_providers
