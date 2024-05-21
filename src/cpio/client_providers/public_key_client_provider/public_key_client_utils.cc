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

#include "public_key_client_utils.h"

#include <locale>
#include <memory>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include "absl/strings/numbers.h"
#include "src/core/interface/http_types.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "error_codes.h"

using google::cmrt::sdk::public_key_service::v1::PublicKey;
using google::scp::core::BytesBuffer;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::HttpHeaders;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::Uri;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED;
using google::scp::core::errors::
    SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED;

namespace {
constexpr std::string_view kPublicKeysLabel = "keys";
constexpr std::string_view kPublicKeyIdLabel = "id";
constexpr std::string_view kPublicKeyLabel = "key";
constexpr std::string_view kPublicKeyHeaderDate = "date";
constexpr std::string_view kPublicKeyHeaderCacheControl = "cache-control";
constexpr std::string_view kPublicKeyDateTimeFormat = "%a, %d %b %Y %H:%M:%S";
constexpr std::string_view kPublicKeyMaxAgeRegex = R"(max-age=(\d+))";
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResult PublicKeyClientUtils::ParseExpiredTimeFromHeaders(
    const HttpHeaders& headers, uint64_t& expired_time_in_s) noexcept {
  auto created_date = headers.find(kPublicKeyHeaderDate);
  auto cache_control = headers.find(kPublicKeyHeaderCacheControl);
  if (created_date == headers.end() || cache_control == headers.end()) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
  }
  std::tm time_date = {};
  std::istringstream stream_time(created_date->second);
  stream_time >> std::get_time(&time_date, kPublicKeyDateTimeFormat.data());
  auto mt_time = std::mktime(&time_date);
  if (mt_time < 0) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
  }
  std::smatch results;
  std::regex max_age_capture_expression(std::string{kPublicKeyMaxAgeRegex});
  if (std::regex_search(cache_control->second, results,
                        max_age_capture_expression)) {
    // Use the capture group text (idx 1), not the entire text match (idx 0):
    std::string max_age_seconds = results.str(1);
    int max_age_val = 0;
    if (absl::SimpleAtoi(max_age_seconds, &max_age_val)) {
      expired_time_in_s = mt_time + max_age_val;
      return SuccessExecutionResult();
    }
  }

  return FailureExecutionResult(
      SC_PUBLIC_KEY_CLIENT_PROVIDER_EXPIRED_TIME_FETCH_FAILED);
}

ExecutionResult PublicKeyClientUtils::ParsePublicKeysFromBody(
    const BytesBuffer& body, std::vector<PublicKey>& public_keys) noexcept {
  auto json_response =
      nlohmann::json::parse(body.bytes->begin(), body.bytes->end());
  auto json_keys = json_response.find(kPublicKeysLabel);
  if (json_keys == json_response.end()) {
    return FailureExecutionResult(
        SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
  }

  auto key_count = json_keys.value().size();
  for (size_t i = 0; i < key_count; ++i) {
    auto json_str = json_keys.value()[i];

    public_keys.emplace_back();
    auto it = json_str.find(kPublicKeyIdLabel);
    if (it == json_str.end()) {
      return FailureExecutionResult(
          SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
    }
    public_keys.back().set_key_id(it.value());

    it = json_str.find(kPublicKeyLabel);
    if (it == json_str.end()) {
      return FailureExecutionResult(
          SC_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEYS_FETCH_FAILED);
    }
    public_keys.back().set_public_key(it.value());
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio::client_providers
