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

#include "absl/status/status.h"
#include "absl/time/time.h"

#include "multi_service_roma_host.h"
#include "test_host_service_roma_host.h"
#include "test_service_romav8_app_service.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrEq;

using ::privacy_sandbox::server_common::TestMethodRequest;
using ::privacy_sandbox::server_common::TestMethodResponse;
using ::privacysandbox::test_server::TestService;

namespace privacysandbox::testserver::roma::AppApi::RomaTestServiceTest {

namespace {
const absl::Duration kDefaultTimeout = absl::Seconds(10);
}

TEST(RomaV8AppTest, EncodeDecodeSimpleProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = TestService<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    TestServer.TestMethod = function(req) {
      return {
        output: req.input + "World. From Callback",
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc->Register(register_finished, register_status, jscode).ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  TestMethodRequest req;
  req.set_input("Hello ");
  TestMethodResponse resp;
  ASSERT_TRUE(app_svc->TestMethod(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  EXPECT_THAT(resp.output(), StrEq("Hello World. From Callback"));
}

TEST(RomaV8AppTest, EncodeDecodeEmptyProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = TestService<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    TestServer.TestMethod = function(req) {
      return {
        output: "",
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc->Register(register_finished, register_status, jscode).ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  TestMethodRequest req;
  TestMethodResponse resp;
  ASSERT_TRUE(app_svc->TestMethod(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  EXPECT_THAT(resp.output(), IsEmpty());
}

TEST(RomaV8AppTest, EncodeDecodeEmptyProtobufWithNoFields) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = TestService<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    TestServer.TestMethod = function(req) {
      return {};
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc->Register(register_finished, register_status, jscode).ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  TestMethodRequest req;
  TestMethodResponse resp;
  ASSERT_TRUE(app_svc->TestMethod(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  EXPECT_THAT(resp.output(), IsEmpty());
}

TEST(RomaV8AppTest, EncodeDecodeProtobufWithNativeCallback) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  privacysandbox::test_host_server::RegisterHostApi(config);
  privacysandbox::multi_server::RegisterHostApi(config);

  auto app_svc = TestService<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    TestServer.TestMethod = function(req) {
      var native_req = {input: req.input};
      var native_res = TestHostServer.NativeMethod(native_req);

      var multi_req = {input: req.input};
      var multi_res = MultiServer.TestMethod1(multi_req);

      var multi_req2 = {input: req.input};
      var multi_res2 = MultiServer.TestMethod2(multi_req2);

      return {
        output: native_res.output + ". " + multi_res.output + ". " + multi_res2.output,
      };
    };
  )";
  absl::Notification register_finished;
  absl::Status register_status;
  ASSERT_TRUE(
      app_svc->Register(register_finished, register_status, jscode).ok());
  register_finished.WaitForNotificationWithTimeout(kDefaultTimeout);
  EXPECT_TRUE(register_status.ok());

  absl::Notification completed;
  TestMethodRequest req;
  req.set_input("Hello ");
  TestMethodResponse resp;
  ASSERT_TRUE(app_svc->TestMethod(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  EXPECT_THAT(
      resp.output(),
      StrEq("Hello World. From NativeMethod. Hello World. From TestMethod1. "
            "Hello World. From TestMethod2"));
}

}  // namespace privacysandbox::testserver::roma::AppApi::RomaTestServiceTest
