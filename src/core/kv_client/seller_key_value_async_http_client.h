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

#ifndef SRC_CORE_KV_CLIENT_SELLER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_
#define SRC_CORE_KV_CLIENT_SELLER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_

#include <memory>
#include <string>

#include "src/core/kv_client/common.h"

namespace privacy_sandbox::server_common::kv_client {

// The data used to build the Seller KV look url suffix
struct GetSellerValuesInput {
  // [SSP] List of keys to query values for, under the namespace renderUrls.
  UrlKeysSet render_urls;

  // [SSP] List of keys to query values for, under the namespace
  // adComponentRenderUrls.
  UrlKeysSet ad_component_render_urls;

  // [SSP] The client type that originated the request. Passed to the key/value
  // service.
  ClientType client_type{CLIENT_TYPE_UNKNOWN};

  // [DSP] Optional ID for experiments conducted by seller. By spec, valid
  // values are in the range: [0, 65535].
  std::string seller_kv_experiment_group_id;
};

// Response from Seller Key Value server.
struct GetSellerValuesOutput {
  // Response JSON string.
  std::string result;
  // Used for instrumentation purposes in upper layers.
  size_t request_size;
  size_t response_size;
  // Optional, indicates version of the KV data.
  uint32_t data_version;
};

// TODO(b/411430242): Move actual implementation from B&A repo. This is
// currently no-op
class SellerKeyValuesAsyncHttpClient
    : public ByosClient<GetSellerValuesInput, GetSellerValuesOutput> {
 public:
  explicit SellerKeyValueAsyncHttpClient(
      std::string_view kv_server_base_address)
      : kv_server_base_address_(kv_server_base_address) {}

  absl::Status Execute(
      std::unique_ptr<GetSellerValuesInput> keys,
      const RequestMetadata& metadata,
      absl::AnyInvocable<
          void(absl::StatusOr<std::unique_ptr<GetSellerValuesOutput>>) &&>
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

#endif  // SRC_CORE_KV_CLIENT_SELLER_KEY_VALUE_ASYNC_HTTP_CLIENT_H_
