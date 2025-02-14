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

#include "aws_v4_signer.h"

#include <time.h>

#include <algorithm>
#include <chrono>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <boost/algorithm/string.hpp>
#include <openssl/hmac.h>
#include <openssl/sha.h>

#include "src/public/core/interface/execution_result.h"

#include "../error_codes.h"

using boost::algorithm::find_nth;
using boost::algorithm::is_any_of;
using boost::algorithm::join;
using boost::algorithm::split;
using boost::algorithm::to_lower_copy;
using boost::algorithm::token_compress_off;
using boost::algorithm::token_compress_on;

namespace {
constexpr std::string_view kEmptyStringSha256 =
    "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855";
constexpr std::string_view kAmzDateHeader = "X-Amz-Date";
constexpr std::string_view kHostHeader = "Host";
constexpr std::string_view kAmzDateFormat = "%Y%m%dT%H%M%SZ";
constexpr std::string_view kAmzSecurityTokenHeader = "X-Amz-Security-Token";
constexpr std::string_view kAuthorizationHeader = "Authorization";
constexpr std::string_view kSigV4Algorithm = "AWS4-HMAC-SHA256";
constexpr std::string_view kHexLookup = "0123456789abcdef";
}  // namespace

namespace google::scp::core {

static std::string HexEncode(unsigned char data[], size_t size) {
  std::string result;
  result.reserve(size * 2);
  for (size_t i = 0; i < size; ++i) {
    auto b = data[i];
    result.push_back(kHexLookup[b >> 4]);    // High 4 bits
    result.push_back(kHexLookup[b & 0x0f]);  // Low 4 bits
  }
  return result;
}

static std::string Sha256(std::string_view data) {
  SHA256_CTX sha256;
  SHA256_Init(&sha256);
  SHA256_Update(&sha256, data.data(), data.size());
  unsigned char hash[SHA256_DIGEST_LENGTH];
  SHA256_Final(hash, &sha256);
  return HexEncode(hash, sizeof(hash));
}

std::vector<unsigned char> HmacSha256(const std::vector<unsigned char>& key,
                                      std::string_view data) {
  unsigned char hmac[EVP_MAX_MD_SIZE];
  unsigned int size = 0;
  HMAC(EVP_sha256(), key.data(), key.size(),
       reinterpret_cast<const unsigned char*>(data.data()), data.length(), hmac,
       &size);
  return std::vector(hmac, hmac + size);
}

AwsV4Signer::AwsV4Signer(std::string aws_access_key, std::string aws_secret_key,
                         std::string aws_security_token,
                         std::string service_name, std::string aws_region)
    : aws_access_key_(std::move(aws_access_key)),
      aws_secret_key_(std::move(aws_secret_key)),
      aws_security_token_(std::move(aws_security_token)),
      service_name_(std::move(service_name)),
      aws_region_(std::move(aws_region)) {}

ExecutionResult AwsV4Signer::SignRequest(
    HttpRequest& http_request, std::string_view headers_to_sign) noexcept {
  std::vector<std::string> headers;
  // split the string, compress adjacent delimiters
  split(headers, headers_to_sign, is_any_of(";, "), token_compress_on);
  return SignRequest(http_request, headers);
}

ExecutionResult AwsV4Signer::GetSignatureParts(
    HttpRequest& http_request, std::vector<std::string>& headers_to_sign,
    std::string& signature, std::string& x_amz_date) noexcept {
  if (headers_to_sign.size() == 0) {
    return FailureExecutionResult(
        errors::SC_HTTP2_CLIENT_AUTH_NO_HEADER_SPECIFIED);
  }
  if (!http_request.headers) {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_BAD_REQUEST);
  }
  auto& headers = http_request.headers;
  if (headers->count(std::string{kAuthorizationHeader}) != 0) {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_ALREADY_SIGNED);
  }
  // Find the "X-Amz-Date" header, if non found, put one.
  auto date_entry = headers->find(std::string{kAmzDateHeader});
  std::string timestamp_value;
  if (date_entry == headers->end()) {
    timestamp_value = GetSigningTime();
    headers->insert({std::string{kAmzDateHeader}, timestamp_value});
  } else {
    timestamp_value = date_entry->second;
  }
  // Find the "Host" header, if non found, try get from the path.
  auto host_entry = headers->find(std::string{kHostHeader});

  if (host_entry == headers->end()) {
    auto& path = *http_request.path;
    std::string host_value;
    size_t start_idx;
    if (path.rfind("http", 0) != 0) {  // if path not starts with http
      return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_BAD_REQUEST);
    }

    if (path.rfind("http://", 0) == 0) {
      start_idx = 7;
    } else if (path.rfind("https://", 0) == 0) {
      start_idx = 8;
    } else {
      return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_BAD_REQUEST);
    }
    auto end_itr = find_nth(path, "/", 2);
    if (end_itr.empty()) {
      host_value = path.substr(start_idx);
    } else {
      host_value = path.substr(
          start_idx, std::distance(path.begin() + start_idx, end_itr.begin()));
    }
    headers->insert({std::string{kHostHeader}, host_value});
  }

  // Sort all headers in headers_to_sign by its all lower cases order.
  std::sort(headers_to_sign.begin(), headers_to_sign.end(),
            [](const std::string& a, const std::string& b) {
              return to_lower_copy(a) < to_lower_copy(b);
            });
  // #1 Create canonical request
  // https://docs.aws.amazon.com/general/latest/gr/sigv4-create-canonical-request.html
  std::string canonical_request;
  auto res =
      CreateCanonicalRequest(canonical_request, http_request, headers_to_sign);
  if (!res) {
    return res;
  }
  // #2 Create string to sign
  // https://docs.aws.amazon.com/general/latest/gr/sigv4-create-string-to-sign.html
  std::stringstream str_to_sign_builder;
  str_to_sign_builder << kSigV4Algorithm << '\n' << timestamp_value << '\n';
  // Take the front part of the timestamp as date.
  std::string date = DateFromTimestamp(timestamp_value);
  str_to_sign_builder << date << '/' << aws_region_ << '/' << service_name_
                      << "/aws4_request\n";
  str_to_sign_builder << Sha256(canonical_request);
  std::string str_to_sign = str_to_sign_builder.str();
  // #3 Calculate signature
  // https://docs.aws.amazon.com/general/latest/gr/sigv4-calculate-signature.html
  std::string signature_hex = CalculateSignature(str_to_sign, date);
  // #4 Add to HTTP request, to be done in callers of this function
  // https://docs.aws.amazon.com/general/latest/gr/sigv4-add-signature-to-request.html

  // Now we are done, return.
  signature.swap(signature_hex);
  x_amz_date.swap(timestamp_value);
  return SuccessExecutionResult();
}

ExecutionResult AwsV4Signer::SignRequest(
    HttpRequest& http_request,
    std::vector<std::string>& headers_to_sign) noexcept {
  std::string signature;
  std::string x_amz_date;
  auto ret =
      GetSignatureParts(http_request, headers_to_sign, signature, x_amz_date);
  if (!ret) {
    return ret;
  }
  std::string date = DateFromTimestamp(x_amz_date);
  // #4 Add to HTTP request
  // https://docs.aws.amazon.com/general/latest/gr/sigv4-add-signature-to-request.html
  AddSignatureHeader(http_request, headers_to_sign, date, signature);

  return SuccessExecutionResult();
}

std::string AwsV4Signer::CalculateSignature(std::string_view string_to_sign,
                                            std::string_view date) noexcept {
  std::string init_key_str = absl::StrCat("AWS4", aws_secret_key_);
  std::vector<unsigned char> secret(init_key_str.begin(), init_key_str.end());
  auto hmac_date = HmacSha256(secret, date);
  auto hmac_region = HmacSha256(hmac_date, aws_region_);
  auto hmac_service = HmacSha256(hmac_region, service_name_);
  auto hmac_signing = HmacSha256(hmac_service, "aws4_request");
  auto signature = HmacSha256(hmac_signing, string_to_sign);
  return HexEncode(signature.data(), signature.size());
}

ExecutionResult AwsV4Signer::CreateCanonicalRequest(
    std::string& canonical_request, HttpRequest& http_request,
    const std::vector<std::string>& headers_to_sign) noexcept {
  std::stringstream canonical_request_builder;
  if (http_request.method == HttpMethod::UNKNOWN) {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_BAD_REQUEST);
  }
  if (!http_request.path || !http_request.headers) {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_BAD_REQUEST);
  }
  if (http_request.method == HttpMethod::GET) {
    canonical_request_builder << "GET\n";
  }
  if (http_request.method == HttpMethod::POST) {
    canonical_request_builder << "POST\n";
  }

  // If path is in the form of "/path/to/resource", use it as-is. Otherwise,
  // assume it is in form of "https://example.com/path/to/resource", and here we
  // extract the path part after the host name by finding the third '/'
  auto& path = *http_request.path;
  std::string canonical_path;
  if (path.length() > 0 && path[0] == '/') {
    canonical_path = path;
  } else {
    auto itr = find_nth(path, "/", 2);
    if (itr.empty()) {
      canonical_path = "/";
    } else {
      canonical_path = path.substr(std::distance(path.begin(), itr.begin()));
    }
  }
  canonical_request_builder << canonical_path << "\n";
  if (http_request.query && http_request.query->size() > 0) {
    // If we have any query parameters, sort them.
    // First, we split by '&';
    std::vector<std::string> query_params;
    static auto predicate = [](std::string::value_type c) { return c == '&'; };
    split(query_params, *http_request.query, predicate, token_compress_off);
    // Then, sort and re-assemble
    std::sort(query_params.begin(), query_params.end());
    std::string sorted_query = join(query_params, "&");
    canonical_request_builder << sorted_query;
  }
  canonical_request_builder << "\n";
  // The "signed headers" in the form of ';' delimited, all lower case string.
  // e.g. content-type;host;x-amz-date
  auto& headers = http_request.headers;
  std::stringstream signed_headers;
  // In the following loop, we produce two things: the header:value strings in
  // the canonical request, and the "signed headers";
  for (const auto& header : headers_to_sign) {
    // There might be multiple values of the same header name, in which case the
    // values are put as a comma delimited list.
    auto header_entry = headers->equal_range(header);
    if (header_entry.first == header_entry.second) {
      // The required header does not exist in http_request. Return error.
      return FailureExecutionResult(
          errors::SC_HTTP2_CLIENT_AUTH_MISSING_HEADER);
    }
    auto header_lower = to_lower_copy(header);
    auto iter = header_entry.first;
    // Put the first header
    canonical_request_builder << header_lower << ':' << iter->second;
    ++iter;
    for (; iter != header_entry.second; ++iter) {
      canonical_request_builder << ',' << iter->second;
    }
    canonical_request_builder << '\n';
    signed_headers << header_lower << ';';
  }
  canonical_request_builder << "\n";

  signed_headers.seekp(-1, std::ios_base::end);  // remove the last semi-colon
  signed_headers << '\n';
  canonical_request_builder << signed_headers.str();
  if (http_request.body != nullptr && !http_request.body->empty()) {
    std::string body(http_request.body->begin(), http_request.body->end());
    canonical_request_builder << Sha256(body);
  } else {
    canonical_request_builder << kEmptyStringSha256;
  }
  canonical_request = canonical_request_builder.str();
  return SuccessExecutionResult();
}

ExecutionResult AwsV4Signer::SignRequestWithSignature(
    HttpRequest& http_request, std::vector<std::string>& headers_to_sign,
    std::string_view x_amz_date, std::string_view signature) noexcept {
  // Sort all headers in headers_to_sign by its all lower cases order.
  std::sort(headers_to_sign.begin(), headers_to_sign.end(),
            [](const std::string& a, const std::string& b) {
              return to_lower_copy(a) < to_lower_copy(b);
            });
  if (!http_request.headers) {
    return FailureExecutionResult(errors::SC_HTTP2_CLIENT_AUTH_BAD_REQUEST);
  }
  // Remove potentially existing X-Amz-Date header, insert designated one.
  http_request.headers->erase(std::string(kAmzDateHeader));
  http_request.headers->insert(
      {std::string(kAmzDateHeader), std::string(x_amz_date)});
  std::string date = DateFromTimestamp(x_amz_date);
  AddSignatureHeader(http_request, headers_to_sign, date, signature);
  return SuccessExecutionResult();
}

void AwsV4Signer::AddSignatureHeader(
    HttpRequest& http_request, const std::vector<std::string>& headers_to_sign,
    std::string_view date, std::string_view signature) {
  auto& headers = http_request.headers;
  std::stringstream auth_header_value_builder;
  auth_header_value_builder
      << kSigV4Algorithm << " Credential=" << aws_access_key_ << '/' << date
      << '/' << aws_region_ << '/' << service_name_ << "/aws4_request, "
      << "SignedHeaders=";
  for (const auto& header : headers_to_sign) {
    auth_header_value_builder << to_lower_copy(header) << ';';
  }
  auth_header_value_builder.seekp(-1, std::ios_base::end);
  auth_header_value_builder << ", Signature=" << signature;
  auto auth_header_value = auth_header_value_builder.str();
  headers->insert({std::string{kAuthorizationHeader}, auth_header_value});

  // If the X-Amz-Security-Token header does not exist, add it.
  std::string token;
  if (!aws_security_token_.empty() &&
      headers->count(std::string{kAmzSecurityTokenHeader}) == 0) {
    headers->insert(
        {std::string{kAmzSecurityTokenHeader}, aws_security_token_});
  }
}

std::string AwsV4Signer::GetSigningTime() {
  auto chrono_now = std::chrono::system_clock::now();
  time_t time_t_now = std::chrono::system_clock::to_time_t(chrono_now);
  struct tm gmt_timestamp;
  gmtime_r(&time_t_now, &gmt_timestamp);
  char formatted_timestamp[64] = {0};
  std::strftime(formatted_timestamp, sizeof(formatted_timestamp),
                kAmzDateFormat.data(), &gmt_timestamp);
  return std::string(formatted_timestamp);
}

}  // namespace google::scp::core
