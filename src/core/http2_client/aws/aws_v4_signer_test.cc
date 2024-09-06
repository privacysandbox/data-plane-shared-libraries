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

#include "src/core/http2_client/aws/aws_v4_signer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <future>
#include <memory>

#include "src/core/http2_client/error_codes.h"
#include "src/core/http2_client/http2_client.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::test::ResultIs;
using ::testing::StrEq;

namespace google::scp::core {

TEST(AwsV4SignerTest, BasicE2E) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"Content-Type", "application/json"});
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");
  std::vector<std::string> headers_to_sign{"Content-Type", "X-Amz-Date",
                                           "Host"};
  signer.SignRequest(request, headers_to_sign);

  static constexpr std::string_view kExpectedHeaderVal =
      "AWS4-HMAC-SHA256 "
      "Credential=OHMYGODALLCAPS4/20220608/us-west-1/execute-api/"
      "aws4_request, SignedHeaders=content-type;host;x-amz-date, "
      "Signature="
      "239227327adbcecca71c595956134c6f3d3567c60e895a1c5c3c4980238b32cb";
  auto iter = request.headers->find("Authorization");
  EXPECT_NE(iter, request.headers->end());
  EXPECT_THAT(iter->second, StrEq(kExpectedHeaderVal));
}

TEST(AwsV4SignerTest, DelimitedHeadersToSign) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"Content-Type", "application/json"});
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");

  std::string headers_to_sign{"Content-Type, X-Amz-Date; Host"};
  signer.SignRequest(request, headers_to_sign);

  static constexpr std::string_view kExpectedHeaderVal =
      "AWS4-HMAC-SHA256 "
      "Credential=OHMYGODALLCAPS4/20220608/us-west-1/execute-api/"
      "aws4_request, SignedHeaders=content-type;host;x-amz-date, "
      "Signature="
      "239227327adbcecca71c595956134c6f3d3567c60e895a1c5c3c4980238b32cb";
  auto iter = request.headers->find("Authorization");
  EXPECT_NE(iter, request.headers->end());
  EXPECT_THAT(iter->second, StrEq(kExpectedHeaderVal));
}

TEST(AwsV4SignerTest, IteratorHeadersToSign) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"Content-Type", "application/json"});
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");

  static const char* headers_to_sign[] = {"Content-Type", "X-Amz-Date", "Host"};
  signer.SignRequest(request, headers_to_sign, headers_to_sign + 3);

  static constexpr std::string_view kExpectedHeaderVal =
      "AWS4-HMAC-SHA256 "
      "Credential=OHMYGODALLCAPS4/20220608/us-west-1/execute-api/"
      "aws4_request, SignedHeaders=content-type;host;x-amz-date, "
      "Signature="
      "239227327adbcecca71c595956134c6f3d3567c60e895a1c5c3c4980238b32cb";
  auto iter = request.headers->find("Authorization");
  EXPECT_NE(iter, request.headers->end());
  EXPECT_THAT(iter->second, StrEq(kExpectedHeaderVal));
}

TEST(AwsV4SignerTest, MissingHeader) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  // Missing the "Content-Type" header here.
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");

  std::vector<std::string> headers_to_sign{"Content-Type", "X-Amz-Date",
                                           "Host"};
  auto res = signer.SignRequest(request, headers_to_sign);
  EXPECT_THAT(res, ResultIs(FailureExecutionResult(
                       errors::SC_HTTP2_CLIENT_AUTH_MISSING_HEADER)));
}

TEST(AwsV4SignerTest, NoHeaderToSign) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");

  std::vector<std::string> headers_to_sign;
  auto res = signer.SignRequest(request, headers_to_sign);
  EXPECT_THAT(res, ResultIs(FailureExecutionResult(
                       errors::SC_HTTP2_CLIENT_AUTH_NO_HEADER_SPECIFIED)));
}

TEST(AwsV4SignerTest, AutoGenerateDate) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");

  std::vector<std::string> headers_to_sign{"X-Amz-Date", "Host"};
  signer.SignRequest(request, headers_to_sign);
  EXPECT_EQ(request.headers->count("X-Amz-Date"), 1);
}

TEST(AwsV4SignerTest, AutoGenerateHost) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>(
      "https://cmhhru8hu0.execute-api.us-west-1.amazonaws.com/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"Content-Type", "application/json"});
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");
  std::vector<std::string> headers_to_sign{"Content-Type", "X-Amz-Date",
                                           "Host"};
  signer.SignRequest(request, headers_to_sign);

  static constexpr std::string_view kExpectedHeaderVal =
      "AWS4-HMAC-SHA256 "
      "Credential=OHMYGODALLCAPS4/20220608/us-west-1/execute-api/"
      "aws4_request, SignedHeaders=content-type;host;x-amz-date, "
      "Signature="
      "239227327adbcecca71c595956134c6f3d3567c60e895a1c5c3c4980238b32cb";
  auto iter = request.headers->find("Authorization");
  EXPECT_NE(iter, request.headers->end());
  EXPECT_THAT(iter->second, StrEq(kExpectedHeaderVal));
}

TEST(AwsV4SignerTest, WithBody) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>("/test/auth");
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"Content-Type", "application/json"});
  request.headers->insert({"X-Amz-Date", "20220608T103745Z"});
  request.headers->insert(
      {"Host", "cmhhru8hu0.execute-api.us-west-1.amazonaws.com"});

  std::string body_str{R"({"foo": "bar"})"};
  request.body.bytes =
      std::make_shared<std::vector<Byte>>(body_str.begin(), body_str.end());
  request.body.length = request.body.bytes->size();

  AwsV4Signer signer("OHMYGODALLCAPS4", "abcdefg1234567/pTxz/FoobarBigSmall",
                     "", "execute-api", "us-west-1");
  std::vector<std::string> headers_to_sign{"Content-Type", "X-Amz-Date",
                                           "Host"};
  signer.SignRequest(request, headers_to_sign);

  std::string canon_req;
  signer.CreateCanonicalRequest(canon_req, request, headers_to_sign);
  EXPECT_THAT(canon_req, StrEq(R"(POST
/test/auth

content-type:application/json
host:cmhhru8hu0.execute-api.us-west-1.amazonaws.com
x-amz-date:20220608T103745Z

content-type;host;x-amz-date
426fc04f04bf8fdb5831dc37bbb6dcf70f63a37e05a68c6ea5f63e85ae579376)"));

  static constexpr std::string_view kExpectedHeaderVal =
      "AWS4-HMAC-SHA256 "
      "Credential=OHMYGODALLCAPS4/20220608/us-west-1/execute-api/"
      "aws4_request, SignedHeaders=content-type;host;x-amz-date, "
      "Signature="
      "2dea4ba14ba1cf625582ab7d5d03f249b5f0b69632436bc943864a360afccffa";
  auto iter = request.headers->find("Authorization");
  EXPECT_NE(iter, request.headers->end());
  EXPECT_THAT(iter->second, StrEq(kExpectedHeaderVal));
}
}  // namespace google::scp::core
