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

#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_PRIVATE_KEY_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_PRIVATE_KEY_CLIENT_PROVIDER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/btree_set.h"
#include "absl/synchronization/mutex.h"
#include "google/protobuf/any.pb.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/http_types.h"
#include "src/cpio/client_providers/interface/kms_client_provider_interface.h"
#include "src/cpio/client_providers/interface/private_key_client_provider_interface.h"
#include "src/cpio/client_providers/interface/private_key_fetcher_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"
#include "private_key_client_utils.h"

namespace google::scp::cpio::client_providers {
/*! @copydoc PrivateKeyClientProviderInterface
 */
class PrivateKeyClientProvider : public PrivateKeyClientProviderInterface {
 public:
  virtual ~PrivateKeyClientProvider() = default;

  explicit PrivateKeyClientProvider(
      PrivateKeyClientOptions private_key_client_options,
      absl::Nonnull<std::unique_ptr<PrivateKeyFetcherProviderInterface>>
          private_key_fetcher,
      absl::Nonnull<std::unique_ptr<KmsClientProviderInterface>> kms_client)
      : private_key_fetcher_(std::move(private_key_fetcher)),
        kms_client_provider_(std::move(kms_client)) {
    endpoint_list_.reserve(1 +
                           private_key_client_options
                               .secondary_private_key_vending_endpoints.size());
    endpoint_list_.push_back(std::move(
        private_key_client_options.primary_private_key_vending_endpoint));
    for (auto& endpoint :
         private_key_client_options.secondary_private_key_vending_endpoints) {
      endpoint_list_.push_back(std::move(endpoint));
    }
    endpoint_count_ = endpoint_list_.size();
  }

  absl::Status ListPrivateKeys(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          context) noexcept override;

 protected:
  /// The overall status of the whole ListPrivateKeys call.
  struct ListPrivateKeysStatus {
    ListPrivateKeysStatus()
        : total_key_split_count(0),
          finished_key_split_count(0),
          fetching_call_returned_count(0),
          got_failure(false) {}

    virtual ~ListPrivateKeysStatus() = default;

    /// List of ExecutionResult.
    std::vector<KeysResultPerEndpoint> result_list;

    /// List of ExecutionResult.
    absl::btree_set<std::string> key_id_set;
    // Mutex to make sure only one thread accessing the key_id_set.
    absl::Mutex set_mutex;

    ListingMethod listing_method = ListingMethod::kByKeyId;

    uint64_t call_count_per_endpoint;

    /// How many key splits fetched in total.
    std::atomic<size_t> total_key_split_count;
    /// How many key splits finished decryption.
    std::atomic<size_t> finished_key_split_count;
    /// How many fetching call returned.
    std::atomic<size_t> fetching_call_returned_count;

    /// whether ListPrivateKeys() call got failure result.
    std::atomic<bool> got_failure;
  };

  /**
   * @brief Is called after FetchPrivateKey is completed.
   *
   * @param list_private_keys_context ListPrivateKeys context.
   * @param fetch_private_key_context FetchPrivateKey context.
   * @param kms_client KMS client instance for current endpoint.
   * @param list_keys_status ListPrivateKeys operation status.
   * @param endpoints_status operation status of endpoints for one key.
   * @param uri_index endpoint index in endpoints vector.
   *
   */
  virtual void OnFetchPrivateKeyCallback(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          list_private_keys_context,
      core::AsyncContext<PrivateKeyFetchingRequest, PrivateKeyFetchingResponse>&
          fetch_private_key_context,
      std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
      size_t uri_index) noexcept;

  /**
   * @brief Is called after Decrypt is completed.
   *
   * @param list_private_keys_context ListPrivateKeys context.
   * @param decrypt_context KMS client Decrypt context.
   * @param list_keys_status ListPrivateKeys operation status.
   * @param endpoints_status operation status of endpoints for one key.
   * @param uri_index endpoint index in endpoints vector.
   *
   */
  virtual void OnDecryptCallback(
      core::AsyncContext<
          cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest,
          cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse>&
          list_private_keys_context,
      core::AsyncContext<cmrt::sdk::kms_service::v1::DecryptRequest,
                         cmrt::sdk::kms_service::v1::DecryptResponse>&
          decrypt_context,
      std::shared_ptr<ListPrivateKeysStatus> list_keys_status,
      std::shared_ptr<EncryptionKey> encryption_key, size_t uri_index) noexcept;

  /// Configurations for PrivateKeyClient.
  PrivateKeyClientOptions private_key_client_options_;

  /// The private key fetching client instance.
  std::unique_ptr<PrivateKeyFetcherProviderInterface> private_key_fetcher_;

  /// KMS client provider.
  std::unique_ptr<KmsClientProviderInterface> kms_client_provider_;

  // This is temp way to collect all endpoints in one vector. Maybe we should
  // change PrivateKeyClientOptions structure to make thing easy.
  std::vector<PrivateKeyVendingEndpoint> endpoint_list_;
  size_t endpoint_count_;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_CLIENT_PROVIDER_PRIVATE_KEY_CLIENT_PROVIDER_H_
