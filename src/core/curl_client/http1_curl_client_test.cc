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
#include "src/core/curl_client/http1_curl_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <utility>

#include "absl/log/check.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/curl_client/error_codes.h"
#include "src/core/curl_client/http1_curl_wrapper.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using testing::AtLeast;  // codespell:ignore AtLeast
using testing::ByMove;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::IsSupersetOf;
using testing::NiceMock;
using testing::Not;
using testing::Pair;
using testing::Pointee;
using testing::Return;

namespace google::scp::core::test {
namespace {

class MockCurlWrapperProvider : public NiceMock<Http1CurlWrapperProvider> {
 public:
  MOCK_METHOD(ExecutionResultOr<std::shared_ptr<Http1CurlWrapper>>, MakeWrapper,
              (), (override));
};

class MockCurlWrapper : public NiceMock<Http1CurlWrapper> {
 public:
  ExecutionResultOr<HttpResponse> PerformRequest(
      const HttpRequest& request, const absl::Duration& timeout) noexcept {
    return PerformRequest(request);
  }

  MOCK_METHOD(ExecutionResultOr<HttpResponse>, PerformRequest,
              (const HttpRequest&));
};

constexpr uint64_t kThreadCount = 4;
constexpr uint64_t kQueueCap = 10;

class Http1CurlClientTest : public ::testing::Test {
 protected:
  Http1CurlClientTest()
      : cpu_async_executor_(kThreadCount, kQueueCap),
        io_async_executor_(kThreadCount, kQueueCap),
        wrapper_(std::make_shared<MockCurlWrapper>()) {
    auto provider = std::make_unique<MockCurlWrapperProvider>();
    provider_ = provider.get();
    constexpr uint64_t delay_duration_ms = 1UL;
    constexpr size_t maximum_allowed_retry_count = 10;
    subject_.emplace(
        &cpu_async_executor_, &io_async_executor_, std::move(provider),
        common::RetryStrategyOptions(common::RetryStrategyType::Exponential,
                                     delay_duration_ms,
                                     maximum_allowed_retry_count));
    ON_CALL(*provider_, MakeWrapper).WillByDefault(Return(wrapper_));
  }

  AsyncExecutor cpu_async_executor_, io_async_executor_;

  std::shared_ptr<MockCurlWrapper> wrapper_;
  MockCurlWrapperProvider* provider_;

  std::optional<Http1CurlClient> subject_;
};

// We only compare the body but we can add more checks if we want.
MATCHER_P(RequestEquals, expected, "") {
  return ExplainMatchResult(arg.body.ToString(), expected.body.ToString(),
                            result_listener);
}

// We only compare the body but we can add more checks if we want.
MATCHER_P(ResponseEquals, expected, "") {
  return ExplainMatchResult(arg.body.ToString(), expected.body.ToString(),
                            result_listener);
}

TEST_F(Http1CurlClientTest, IssuesPerformRequestOnWrapper) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body = BytesBuffer("buf");

  HttpRequest expected_request;
  expected_request.body = BytesBuffer("buf");
  HttpResponse response;
  response.body = BytesBuffer("resp");
  EXPECT_CALL(*wrapper_, PerformRequest(RequestEquals(expected_request)))
      .WillOnce(Return(response));

  absl::Notification finished;
  http_context.callback = [&response, &finished](auto& http_context) {
    ASSERT_SUCCESS(http_context.result);
    EXPECT_THAT(http_context.response, Pointee(ResponseEquals(response)));
    finished.Notify();
  };

  ASSERT_THAT(subject_->PerformRequest(http_context), IsSuccessful());

  finished.WaitForNotification();
}

TEST_F(Http1CurlClientTest, RetriesWork) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body = BytesBuffer("buf");

  HttpRequest expected_request;
  expected_request.body = BytesBuffer("buf");
  HttpResponse response;
  response.body = BytesBuffer("resp");

  // Fail 3 times, then succeed.
  {
    InSequence seq;
    EXPECT_CALL(*wrapper_, PerformRequest)
        .Times(3)
        .WillRepeatedly(Return(
            RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FAILED)));
    EXPECT_CALL(*wrapper_, PerformRequest(RequestEquals(expected_request)))
        .WillOnce(Return(response));
  }

  absl::Notification finished;
  http_context.callback = [&response, &finished](auto& http_context) {
    ASSERT_SUCCESS(http_context.result);
    EXPECT_THAT(http_context.response, Pointee(ResponseEquals(response)));
    finished.Notify();
  };

  ASSERT_THAT(subject_->PerformRequest(http_context), IsSuccessful());

  finished.WaitForNotification();
}

TEST_F(Http1CurlClientTest, FailureEnds) {
  AsyncContext<HttpRequest, HttpResponse> http_context;
  http_context.request = std::make_shared<HttpRequest>();
  http_context.request->body = BytesBuffer("buf");

  HttpRequest expected_request;
  expected_request.body = BytesBuffer("buf");
  HttpResponse response;
  response.body = BytesBuffer("resp");

  EXPECT_CALL(*wrapper_, PerformRequest)
      .Times(AtLeast(2))  // codespell:ignore AtLeast
      .WillRepeatedly(
          Return(RetryExecutionResult(errors::SC_CURL_CLIENT_REQUEST_FAILED)));

  absl::Notification finished;
  http_context.callback = [&finished](auto& context) {
    EXPECT_THAT(context.result, Not(IsSuccessful()));
    finished.Notify();
  };

  ASSERT_THAT(subject_->PerformRequest(http_context), IsSuccessful());

  finished.WaitForNotification();
}

}  // namespace
}  // namespace google::scp::core::test
