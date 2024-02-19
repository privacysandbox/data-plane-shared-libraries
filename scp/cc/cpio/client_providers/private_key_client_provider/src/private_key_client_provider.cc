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

#include "core/common/concurrent_map/src/concurrent_map.h"
#include "core/interface/async_context.h"
#include "core/interface/http_client_interface.h"
#include "core/interface/http_types.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/private_key_client/type_def.h"
#include "public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

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
using google::scp::core::common::ConcurrentMap;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS;

static constexpr char kPrivateKeyClientProvider[] = "PrivateKeyClientProvider";

namespace google::scp::cpio::client_providers {
ExecutionResult PrivateKeyClientProvider::Init() noexcept {
  endpoint_list_.push_back(
      private_key_client_options_->primary_private_key_vending_endpoint);
  for (const auto& endpoint :
       private_key_client_options_->secondary_private_key_vending_endpoints) {
    if (!endpoint.private_key_vending_service_endpoint.empty())
      endpoint_list_.push_back(endpoint);
  }
  endpoint_count_ = endpoint_list_.size();

  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult PrivateKeyClientProvider::ListPrivateKeys(
    AsyncContext<ListPrivateKeysRequest, ListPrivateKeysResponse>&
        list_private_keys_context) noexcept {
  auto list_keys_status = std::make_shared<ListPrivateKeysStatus>();
  list_keys_status->listing_method =
      list_private_keys_context.request->key_ids().empty()
          ? ListingMethod::kByMaxAge
          : ListingMethod::kByKeyId;
  list_keys_status->result_list =
      std::vector<KeysResultPerEndpoint>(endpoint_count_);

  list_keys_status->call_count_per_endpoint =
      list_keys_status->listing_method == ListingMethod::kByKeyId
          ? list_private_keys_context.request->key_ids().size()
          : 1;

  for (size_t call_index = 0;
       call_index < list_keys_status->call_count_per_endpoint; ++call_index) {
    for (size_t uri_index = 0; uri_index < endpoint_count_; ++uri_index) {
      auto request = std::make_shared<PrivateKeyFetchingRequest>();

      if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
        request->key_id = std::make_shared<std::string>(
            list_private_keys_context.request->key_ids(call_index));
      } else {
        request->max_age_seconds =
            list_private_keys_context.request->max_age_seconds();
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
          list_private_keys_context.result = execution_result;
          SCP_ERROR(kPrivateKeyClientProvider, kZeroUuid, execution_result,
                    "Failed to fetch private key with endpoint %s.",
                    endpoint.private_key_vending_service_endpoint.c_str());
          list_private_keys_context.Finish();
        }

        return execution_result;
      }
    }
  }

  return SuccessExecutionResult();
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
    ExecutionResult out;
    if (auto insert_result =
            list_keys_status->result_list[uri_index]
                .fetch_result_key_id_map.Insert(
                    std::make_pair(*fetch_private_key_context.request->key_id,
                                   execution_result),
                    out);
        !insert_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = insert_result;
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to insert fetch result");
        list_private_keys_context.Finish();
      }
      return;
    }
    // For ListByKeyId, store the key IDs no matter the fetching failed or not.
    list_keys_status->set_mutex.lock();
    list_keys_status->key_id_set.insert(
        *fetch_private_key_context.request->key_id);
    list_keys_status->set_mutex.unlock();
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
      list_keys_status->set_mutex.lock();
      list_keys_status->key_id_set.insert(*encryption_key->key_id);
      list_keys_status->set_mutex.unlock();
    }
    DecryptRequest kms_decrypt_request;
    execution_result = PrivateKeyClientUtils::GetKmsDecryptRequest(
        encryption_key, kms_decrypt_request);
    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to get the key data.");
        list_private_keys_context.Finish();
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
    execution_result = kms_client_provider_->Decrypt(decrypt_context);

    if (!execution_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = execution_result;
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to send decrypt request.");
        list_private_keys_context.Finish();
      }
      return;
    }
  }
}

ExecutionResult InsertDecryptResult(
    ConcurrentMap<std::string, DecryptResult>& decrypt_result_key_id_map,
    EncryptionKey encryption_key, ExecutionResult result,
    std::string plaintext) {
  DecryptResult decrypt_result;
  decrypt_result.decrypt_result = std::move(result);
  decrypt_result.encryption_key = std::move(encryption_key);
  if (!plaintext.empty()) {
    decrypt_result.plaintext = std::move(plaintext);
  }

  DecryptResult out;
  RETURN_AND_LOG_IF_FAILURE(
      decrypt_result_key_id_map.Insert(
          std::make_pair(*decrypt_result.encryption_key.key_id, decrypt_result),
          out),
      kPrivateKeyClientProvider, kZeroUuid, "Failed to insert decrypt result");
  return SuccessExecutionResult();
}

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
    if (auto insert_result = InsertDecryptResult(
            list_keys_status->result_list[uri_index].decrypt_result_key_id_map,
            *encryption_key, std::move(decrypt_context.result),
            std::move(plaintext));
        !insert_result.Successful()) {
      auto got_failure = false;
      if (list_keys_status->got_failure.compare_exchange_strong(got_failure,
                                                                true)) {
        list_private_keys_context.result = insert_result;
        SCP_ERROR_CONTEXT(kPrivateKeyClientProvider, list_private_keys_context,
                          list_private_keys_context.result,
                          "Failed to insert decrypt result.");
        list_private_keys_context.Finish();
      }
      return;
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
        success_decrypt_result.emplace_back(
            std::move(single_party_key.value()));
      } else {
        // If doesn't contain single party key, validate every fetch and
        // decrypt results.
        auto execution_result = PrivateKeyClientUtils::ExtractAnyFailure(
            list_keys_status->result_list, key_id);
        if (!execution_result.Successful()) {
          list_private_keys_context.result = execution_result;
          SCP_ERROR_CONTEXT(
              kPrivateKeyClientProvider, list_private_keys_context,
              list_private_keys_context.result,
              "Failed to fetch the private key for key ID: %s", key_id.c_str());
          list_private_keys_context.Finish();
          return;
        }
        // Key splits returned from each endpoint should match the endpoint
        // count.
        for (int i = 0; i < endpoint_count_; ++i) {
          DecryptResult decrypt_result;
          auto find_result =
              list_keys_status->result_list[i].decrypt_result_key_id_map.Find(
                  key_id, decrypt_result);
          if (!find_result.Successful() ||
              decrypt_result.encryption_key.key_data.size() !=
                  endpoint_count_) {
            if (list_keys_status->listing_method == ListingMethod::kByKeyId) {
              list_private_keys_context.result = FailureExecutionResult(
                  SC_PRIVATE_KEY_CLIENT_PROVIDER_UNMATCHED_ENDPOINTS_SPLITS);
              list_private_keys_context.Finish();
              SCP_ERROR_CONTEXT(
                  kPrivateKeyClientProvider, list_private_keys_context,
                  list_private_keys_context.result,
                  "Unmatched endpoints number and private key  split "
                  "data size for key ID %s.",
                  encryption_key->key_id->c_str());
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
              all_splits_are_available = false;
              break;
            }
          }
          success_decrypt_result.emplace_back(decrypt_result);
        }
      }
      if (all_splits_are_available) {
        auto private_key_or =
            PrivateKeyClientUtils::ConstructPrivateKey(success_decrypt_result);
        if (!private_key_or.Successful()) {
          list_private_keys_context.result = private_key_or.result();
          SCP_ERROR_CONTEXT(kPrivateKeyClientProvider,
                            list_private_keys_context,
                            list_private_keys_context.result,
                            "Failed to construct private key.");
          list_private_keys_context.Finish();
          return;
        }
        *list_private_keys_context.response->add_private_keys() =
            std::move(*private_key_or);
      }
    }

    list_private_keys_context.result = SuccessExecutionResult();
    list_private_keys_context.Finish();
  }
}

std::shared_ptr<PrivateKeyClientProviderInterface>
PrivateKeyClientProviderFactory::Create(
    const std::shared_ptr<PrivateKeyClientOptions>& options,
    const std::shared_ptr<core::HttpClientInterface>& http_client,
    const std::shared_ptr<RoleCredentialsProviderInterface>&
        role_credentials_provider,
    const std::shared_ptr<AuthTokenProviderInterface>& auth_token_provider,
    const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor) {
  auto kms_client_provider = KmsClientProviderFactory::Create(
      std::make_shared<KmsClientOptions>(), role_credentials_provider,
      io_async_executor);
  auto private_key_fetcher = PrivateKeyFetcherProviderFactory::Create(
      http_client, role_credentials_provider, auth_token_provider);

  return std::make_shared<PrivateKeyClientProvider>(
      options, http_client, private_key_fetcher, kms_client_provider);
}

}  // namespace google::scp::cpio::client_providers
