/*
 * Portions Copyright (c) Microsoft Corporation
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

#ifndef SRC_CPP_ACCESSTOKEN_FETCHER_FACTORY_H_
#define SRC_CPP_ACCESSTOKEN_FETCHER_FACTORY_H_

#include <memory>
#include <tuple>

#include "absl/synchronization/notification.h"
#include "public/core/interface/execution_result.h"
#include "scp/cc/core/curl_client/src/http1_curl_client.h"
#include "scp/cc/core/interface/http_client_interface.h"
constexpr absl::Duration kRequestTimeout = absl::Seconds(10);

namespace privacy_sandbox::server_common {
using AccessTokenServiceEndpoint = std::string;
using ClientApplicationId = std::string;
using ClientSecret = std::string;
using ApiIdentifierUri = std::string;
using AccessTokenValue = std::string;

struct AccessToken {
  AccessTokenValue accessToken;
};

struct GetAccessTokenResponse {
  std::shared_ptr<std::string> accesstoken;
};

struct GetAccessTokenRequest {};

struct AccessTokenClientOptions {
  virtual ~AccessTokenClientOptions() = default;

  AccessTokenServiceEndpoint endpoint;
  ClientApplicationId clientid;
  ClientSecret clientSecret;
  ApiIdentifierUri apiApplicationId;
};

class AccessTokenClientFactory {
 public:
  AccessTokenClientFactory(
      const AccessTokenClientOptions& options,
      std::shared_ptr<google::scp::core::HttpClientInterface>
          http_client) noexcept;

  static std::unique_ptr<AccessTokenClientFactory> Create(
      const AccessTokenClientOptions& options,
      std::shared_ptr<google::scp::core::HttpClientInterface>
          http_client) noexcept;

  std::tuple<google::scp::core::ExecutionResult, std::string, int> MakeRequest(
      const std::string& url,
      google::scp::core::HttpMethod method = google::scp::core::HttpMethod::GET,
      const absl::btree_multimap<std::string, std::string>& headers = {},
      std::string body = "");

  std::tuple<google::scp::core::ExecutionResult, std::string, int>
  GetAccessToken();

 private:
  AccessTokenClientOptions tokenOptions_;
  std::shared_ptr<google::scp::core::HttpClientInterface> http_client_;
};
}  // namespace privacy_sandbox::server_common

#endif  // SRC_CPP_ACCESSTOKEN_FETCHER_FACTORY_H_
