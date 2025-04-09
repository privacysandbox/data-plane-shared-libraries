/*
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_CORE_KV_CLIENT_BUYER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_
#define SRC_CORE_KV_CLIENT_BUYER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_

#include <memory>
#include <string>

#include "src/core/kv_client/byos_client.h"
#include "src/core/kv_client/common.h"

namespace privacy_sandbox::server_common::kv_client {

// The data used to build the Buyer KV look url suffix
struct GetBuyerValuesInput {
  // [DSP] List of keys to query values for, under the namespace keys.
  UrlKeysSet keys;

  // [DSP] List of interest group names for which to query values.
  UrlKeysSet interest_group_names;

  // [DSP] The browser sets the hostname of the publisher page to be the value.
  std::string hostname;

  // [DSP] The client type that originated the request. Passed to the key/value
  // service.
  ClientType client_type{ClientType::CLIENT_TYPE_UNKNOWN};

  // [DSP] Optional ID for experiments conducted by buyer. By spec, valid values
  // are [0, 65535].
  std::string buyer_kv_experiment_group_id;

  // [DSP] Optional slot_size if `trustedBiddingSignalsSlotSizeMode` was set to
  // `slot-size`.
  std::string slot_size;

  // [DSP] Optional all_slots_requested_size if
  // `trustedBiddingSignalsSlotSizeMore` was set to `all-slots-requested-sizes`
  std::string all_slots_requested_size;
};

// Response from Buyer Key Value server.
struct GetBuyerValuesOutput {
  // Response JSON string.
  std::string result;
  // Used for instrumentation purposes in upper layers.
  size_t request_size;
  size_t response_size;
  // Optional, indicates version of the KV data.
  uint32_t data_version;
  // By default this should be false
  // However, AdTechs can indicate through a header that is ultimately
  // mapped to this flag, that they don't want to make a downstream v2
  // request and the B&A server should process the returned v1 BYOS response.
  bool is_hybrid_v1_return = false;
};

// TODO(b/411430242): Move actual implementation from B&A repo. This is
// currently no-op
class BuyerKeyValuesAsyncHttpClient
    : public ByosClient<GetBuyerValuesInput, GetBuyerValuesOutput> {
 public:
  explicit BuyerKeyValueAsyncHttpClient(std::string_view kv_server_base_address)
      : kv_server_base_address_(kv_server_base_address) {}

  absl::Status Execute(
      std::unique_ptr<GetBuyerValuesInput> keys,
      const RequestMetadata& metadata,
      absl::AnyInvocable<
          void(absl::StatusOr<std::unique_ptr<GetBuyerValuesOutput>>) &&>
          on_done,
      absl::Duration timeout,
      privacy_sandbox::server_common::log::PSLogContext& log_context =
          const_cast<privacy_sandbox::server_common::log::NoOpContext&>(
              privacy_sandbox::server_common::log::kNoOpContext))
      const override {
    return absl::UnimplementedError("Method not implemented");
  };

 private:
  const std::string kv_server_base_address_;
};

}  // namespace privacy_sandbox::server_common::kv_client

#endif  // SRC_CORE_KV_CLIENT_BUYER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_
