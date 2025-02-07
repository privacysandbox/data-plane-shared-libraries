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
#include "src/core/curl_client/http1_curl_wrapper.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <tuple>
#include <utility>

#include "src/core/curl_client/error_codes.h"
#include "src/core/test/utils/http1_helper/test_http1_server.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using boost::beast::http::status;
using ::testing::IsEmpty;
using ::testing::IsSupersetOf;
using ::testing::Pair;
using ::testing::StrEq;

namespace google::scp::core::test {
namespace {

// Intentionally place a null character before the end and don't place one at
// the end to test null handling. Note that std::string does *not* automatically
// place null characters at the end, except when c_str is called. However, I
// confirmed printing behaves nicely.
constexpr Byte kRequestBody[] = {'a', 'b', '\0', 'c'};

class Http1CurlWrapperTest
    : public ::testing::TestWithParam<std::tuple<status, ExecutionResult>> {
 protected:
  Http1CurlWrapperTest()
      : response_body_(std::string(100'000, '*')),
        post_request_body_(kRequestBody, sizeof(kRequestBody)),
        subject_([]() {
          ExecutionResultOr<std::shared_ptr<Http1CurlWrapper>> wrapper =
              Http1CurlWrapper::MakeWrapper();
          CHECK(wrapper.Successful()) << "Failed to create Http1CurlWrapper";
          return std::move(*wrapper);
        }()) {}

  status GetResponseStatusToReturn() { return std::get<0>(GetParam()); }

  ExecutionResult GetExpectedResult() { return std::get<1>(GetParam()); }

 protected:
  std::string response_body_;
  std::string post_request_body_;
  std::shared_ptr<Http1CurlWrapper> subject_;
  TestHttp1Server server_;
};

TEST_F(Http1CurlWrapperTest, GetWorks) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = std::make_shared<Uri>(server_.GetPath());

  server_.SetResponseBody(BytesBuffer(post_request_body_));

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
  EXPECT_THAT(response->body.ToString(), StrEq(post_request_body_));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
}

TEST_F(Http1CurlWrapperTest, GetWorksBigResponse) {
  std::string reponse_body;
  // Set to 1,000,001 explicitly since that's the value of MAX_RTP_BUFFERSIZE
  // http://shortn/_nvYv40ncLg
  for (int i = 0; i < 1'000'001; ++i) {
    reponse_body += static_cast<char>(i % 255);
  }

  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = std::make_shared<Uri>(server_.GetPath());
  server_.SetResponseBody(BytesBuffer(reponse_body));

  auto response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
  EXPECT_EQ(response->body.ToString(), reponse_body);
  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
}

TEST_F(Http1CurlWrapperTest, GetWorksWithHeaders) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = std::make_shared<Uri>(server_.GetPath());
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"key1", "val1"});
  request.headers->insert({"key2", "val2"});

  server_.SetResponseBody(BytesBuffer(response_body_));
  server_.SetResponseHeaders(
      HttpHeaders({{"resp1", "resp_val1"}, {"resp2", "resp_val2"}}));

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
  EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

  EXPECT_THAT(*response->headers, IsSupersetOf({Pair("resp1", "resp_val1"),
                                                Pair("resp2", "resp_val2")}));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);

  EXPECT_THAT(GetRequestHeadersMap(server_.Request()),
              IsSupersetOf({Pair("key1", "val1"), Pair("key2", "val2")}));
}

TEST_F(Http1CurlWrapperTest, PostWorks) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>(server_.GetPath());
  request.body = BytesBuffer(post_request_body_);

  server_.SetResponseBody(BytesBuffer(response_body_));

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
  EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::post);
  EXPECT_THAT(server_.RequestBody(), StrEq(post_request_body_));
}

TEST_F(Http1CurlWrapperTest, PutWorks) {
  HttpRequest request;
  request.method = HttpMethod::PUT;
  request.path = std::make_shared<Uri>(server_.GetPath());
  request.body = BytesBuffer(post_request_body_);

  server_.SetResponseBody(BytesBuffer(response_body_));

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
  EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::put);
  EXPECT_THAT(server_.RequestBody(), StrEq(post_request_body_));
}

TEST_F(Http1CurlWrapperTest, PostWorksWithHeaders) {
  HttpRequest request;
  request.method = HttpMethod::POST;
  request.path = std::make_shared<Uri>(server_.GetPath());
  request.body = BytesBuffer(post_request_body_);
  request.headers = std::make_shared<HttpHeaders>();
  request.headers->insert({"key1", "val1"});
  request.headers->insert({"key2", "val2"});

  server_.SetResponseBody(BytesBuffer(response_body_));
  server_.SetResponseHeaders(
      HttpHeaders({{"resp1", "resp_val1"}, {"resp2", "resp_val2"}}));

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
  EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

  EXPECT_THAT(*response->headers, IsSupersetOf({Pair("resp1", "resp_val1"),
                                                Pair("resp2", "resp_val2")}));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::post);
  EXPECT_THAT(server_.RequestBody(), StrEq(post_request_body_));

  EXPECT_THAT(GetRequestHeadersMap(server_.Request()),
              IsSupersetOf({Pair("key1", "val1"), Pair("key2", "val2")}));
}

TEST_F(Http1CurlWrapperTest, SingleQueryIsEscaped) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = std::make_shared<Uri>(server_.GetPath());
  request.query = std::make_shared<std::string>("foo=!@#$");

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
  // '=' should not be escaped.
  EXPECT_THAT(server_.Request().target(), StrEq("/?foo=%21%40%23%24"));
}

TEST_F(Http1CurlWrapperTest, MultiQueryIsEscaped) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = std::make_shared<Uri>(server_.GetPath());
  request.query = std::make_shared<std::string>("foo=!@#$&bar=%^()");

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, IsSuccessful());
  EXPECT_EQ(response->code, errors::HttpStatusCode::OK);

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
  // '=' should not be escaped.
  EXPECT_THAT(server_.Request().target(),
              StrEq("/?foo=%21%40%23%24&bar=%25%5E%28%29"));
}

TEST_F(Http1CurlWrapperTest, GetPostGetWorks) {
  // Get 1.
  {
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = std::make_shared<Uri>(server_.GetPath());

    server_.SetResponseBody(BytesBuffer(response_body_));

    ExecutionResultOr<HttpResponse> response =
        subject_->PerformRequest(request);
    ASSERT_THAT(response, IsSuccessful());
    EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
    EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

    EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
    EXPECT_THAT(server_.RequestBody(), IsEmpty());
  }
  // Post.
  {
    HttpRequest request;
    request.method = HttpMethod::POST;
    request.path = std::make_shared<Uri>(server_.GetPath());
    request.body = BytesBuffer(post_request_body_);

    server_.SetResponseBody(BytesBuffer(response_body_));

    ExecutionResultOr<HttpResponse> response =
        subject_->PerformRequest(request);
    ASSERT_THAT(response, IsSuccessful());
    EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
    EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

    EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::post);
    EXPECT_THAT(server_.RequestBody(), StrEq(post_request_body_));
  }
  // Get 2.
  {
    HttpRequest request;
    request.method = HttpMethod::GET;
    request.path = std::make_shared<Uri>(server_.GetPath());

    server_.SetResponseBody(BytesBuffer(response_body_));

    ExecutionResultOr<HttpResponse> response =
        subject_->PerformRequest(request);
    ASSERT_THAT(response, IsSuccessful());
    EXPECT_EQ(response->code, errors::HttpStatusCode::OK);
    EXPECT_THAT(response->body.ToString(), StrEq(response_body_));

    EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
    EXPECT_THAT(server_.RequestBody(), IsEmpty());
  }
}

TEST_P(Http1CurlWrapperTest, PropagatesHttpError) {
  HttpRequest request;
  request.method = HttpMethod::GET;
  request.path = std::make_shared<Uri>(server_.GetPath());

  server_.SetResponseStatus(GetResponseStatusToReturn());

  ExecutionResultOr<HttpResponse> response = subject_->PerformRequest(request);
  ASSERT_THAT(response, ResultIs(GetExpectedResult()));

  EXPECT_EQ(server_.Request().method(), boost::beast::http::verb::get);
}

INSTANTIATE_TEST_SUITE_P(
    HttpErrors, Http1CurlWrapperTest,
    testing::Values(
        std::make_tuple(status::unauthorized,
                        FailureExecutionResult(
                            errors::SC_CURL_CLIENT_REQUEST_UNAUTHORIZED)),
        std::make_tuple(
            status::forbidden,
            FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FORBIDDEN)),
        std::make_tuple(
            status::not_found,
            FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_NOT_FOUND)),
        std::make_tuple(
            status::conflict,
            FailureExecutionResult(errors::SC_CURL_CLIENT_REQUEST_CONFLICT)),
        std::make_tuple(
            status::internal_server_error,
            RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_SERVER_ERROR)),
        std::make_tuple(status::not_implemented,
                        RetryExecutionResult(
                            errors::SC_CURL_CLIENT_REQUEST_NOT_IMPLEMENTED)),
        std::make_tuple(
            status::service_unavailable,
            RetryExecutionResult(
                errors::SC_CURL_CLIENT_REQUEST_SERVICE_UNAVAILABLE)),
        // This one is not enumerated.
        std::make_tuple(status::payment_required,
                        RetryExecutionResult(
                            errors::SC_CURL_CLIENT_REQUEST_OTHER_HTTP_ERROR)),
        std::make_tuple(status::ok, SuccessExecutionResult())));

}  // namespace
}  // namespace google::scp::core::test
