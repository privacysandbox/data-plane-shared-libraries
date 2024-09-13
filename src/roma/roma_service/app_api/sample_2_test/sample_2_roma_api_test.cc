/*
 * Copyright 2023 Google LLC
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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/time/time.h"

#include "sample_2_romav8_app_service.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrEq;

using ::privacysandbox::roma::app_api::sample_2_test::v1::GetSample2Request;
using ::privacysandbox::roma::app_api::sample_2_test::v1::GetSample2Response;
using ::privacysandbox::roma::app_api::sample_2_test::v1::V8Sample2Service;

namespace privacysandbox::kvserver::roma::AppApi::RomaKvTest {

namespace {
const absl::Duration kDefaultTimeout = absl::Seconds(10);
constexpr std::string_view kCodeVersion = "v1";
}  // namespace

TEST(RomaV8AppTest, EncodeDecodeSimpleProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample2Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample2Server.GetSample2 = function(req) {
      return {
        abc: 123,
        unknown1: "An unknown field",
        foo: { foobar1: 123 },
        value: "Something in the reply",
        vals: ["Hi Foobar!"],
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc
          ->Register(jscode, kCodeVersion, register_finished, register_status)
          .ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  GetSample2Request req;
  absl::StatusOr<std::unique_ptr<GetSample2Response>> resp;
  ASSERT_TRUE(app_svc->GetSample2(completed, req, resp).ok());
  ASSERT_TRUE(completed.WaitForNotificationWithTimeout(kDefaultTimeout));

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->vals(), Contains("Hi Foobar!"));
  EXPECT_THAT((*resp)->value(), StrEq("Something in the reply"));
  EXPECT_THAT((*resp)->foo().foobar1(), Eq(123));
  EXPECT_THAT((*resp)->foos_size(), Eq(0));
}

TEST(RomaV8AppTest, EncodeDecodeEmptyProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample2Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample2Server.GetSample2 = function(req) {
      return {
        foo: {},
        foos: [],
        value: "",
        vals: [],
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc
          ->Register(jscode, kCodeVersion, register_finished, register_status)
          .ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  GetSample2Request req;
  absl::StatusOr<std::unique_ptr<GetSample2Response>> resp;
  ASSERT_TRUE(app_svc->GetSample2(completed, req, resp).ok());
  ASSERT_TRUE(completed.WaitForNotificationWithTimeout(kDefaultTimeout));

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->vals(), IsEmpty());
  EXPECT_THAT((*resp)->value(), IsEmpty());
  EXPECT_THAT((*resp)->foo().foobar1(), Eq(0));
  ASSERT_THAT((*resp)->foos_size(), Eq(0));
}

TEST(RomaV8AppTest, EncodeDecodeRepeatedMessageProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample2Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample2Server.GetSample2 = function(req) {
      return {
        vals: [
          "str1",
          "str2",
          "str1",
          "str2",
        ],
        foos: [
          { foobar1: 123 },
          { foobar2: "abc123" },
        ],
        abc: 123,
        unknown1: "An unknown field",
        foo: { foobar1: 123 },
        value: "Something in the reply",
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc
          ->Register(jscode, kCodeVersion, register_finished, register_status)
          .ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  GetSample2Request req;
  absl::StatusOr<std::unique_ptr<GetSample2Response>> resp;
  ASSERT_TRUE(app_svc->GetSample2(completed, req, resp).ok());
  ASSERT_TRUE(completed.WaitForNotificationWithTimeout(kDefaultTimeout));

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->vals_size(), Eq(4));
  ASSERT_THAT((*resp)->foos_size(), Eq(2));
  EXPECT_THAT((*resp)->foos(0).foobar1(), Eq(123));
  EXPECT_THAT((*resp)->foos(1).foobar2(), StrEq("abc123"));
}

TEST(RomaV8AppTest, UseRequestField) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample2Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample2Server.GetSample2 = function(req) {
      return {
        value: "Something in the reply",
        vals: ["Hi " + req.keysList[0] + "!"],
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc
          ->Register(jscode, kCodeVersion, register_finished, register_status)
          .ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  GetSample2Request req;
  req.set_key1("abc123");
  req.set_key2("def123");
  req.add_keys("Foobar");
  absl::StatusOr<std::unique_ptr<GetSample2Response>> resp;
  ASSERT_TRUE(app_svc->GetSample2(completed, req, resp).ok());
  ASSERT_TRUE(completed.WaitForNotificationWithTimeout(kDefaultTimeout));

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->vals(), Contains("Hi Foobar!"));
  EXPECT_THAT((*resp)->value(), StrEq("Something in the reply"));
}

TEST(RomaV8AppTest, CallbackBasedExecute) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample2Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample2Server.GetSample2 = function(req) {
      return {
        value: "Something in the reply",
        vals: ["Hi " + req.keysList[0] + "!"],
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc
          ->Register(jscode, kCodeVersion, register_finished, register_status)
          .ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  GetSample2Request req;
  req.set_key1("abc123");
  req.set_key2("def123");
  req.add_keys("Foobar");

  absl::Notification completed;
  absl::StatusOr<GetSample2Response> response;
  auto callback = [&response,
                   &completed](absl::StatusOr<GetSample2Response> resp) {
    response = std::move(resp);
    completed.Notify();
  };
  ASSERT_TRUE(app_svc->GetSample2(callback, req).ok());
  ASSERT_TRUE(completed.WaitForNotificationWithTimeout(kDefaultTimeout));

  ASSERT_TRUE(response.ok());
  EXPECT_THAT(response->vals(), Contains("Hi Foobar!"));
  EXPECT_THAT(response->value(), StrEq("Something in the reply"));
}

}  // namespace privacysandbox::kvserver::roma::AppApi::RomaKvTest
