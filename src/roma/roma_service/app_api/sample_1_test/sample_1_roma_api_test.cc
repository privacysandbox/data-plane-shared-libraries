/*
 * Copyright 2024 Google LLC
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
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"

#include "sample_1_romav8_app_service.h"

using ::testing::Contains;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::StrEq;

using ::privacysandbox::roma::app_api::sample_1_test::v1::RunSample1Request;
using ::privacysandbox::roma::app_api::sample_1_test::v1::RunSample1Response;
using ::privacysandbox::roma::app_api::sample_1_test::v1::V8Sample1Service;

namespace privacysandbox::sample::roma::AppApi::RomaSample1Test {

namespace {
const absl::Duration kDefaultTimeout = absl::Seconds(10);
constexpr std::string_view kCodeVersion = "v1";
}  // namespace

TEST(RomaV8AppTest, EncodeDecodeSimpleProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample1Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample1Server.RunSample1 = function(req) {
      return {
        response: [
          { model_path: "a/b/c/1/2/3" },
        ],
        unknown1: "An unknown field",
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
  RunSample1Request req;
  absl::StatusOr<std::unique_ptr<RunSample1Response>> resp;
  ASSERT_TRUE(app_svc->RunSample1(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->response_size(), Eq(1));
  EXPECT_THAT((*resp)->response(0).model_path(), StrEq("a/b/c/1/2/3"));
}

TEST(RomaV8AppTest, EncodeDecodeEmptyProtobuf) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample1Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample1Server.RunSample1 = function(req) {
      return {
        response: [],
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
  RunSample1Request req;
  absl::StatusOr<std::unique_ptr<RunSample1Response>> resp;
  ASSERT_TRUE(app_svc->RunSample1(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->response(), IsEmpty());
}

TEST(RomaV8AppTest, UseRequestField) {
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app_svc = V8Sample1Service<>::Create(std::move(config));
  EXPECT_TRUE(app_svc.ok());

  constexpr std::string_view jscode = R"(
    Sample1Server.RunSample1 = function(req) {
      return {
        response: [
          {
            model_path: `foo-${req.requestList[0].modelPath}-bar`,
          },
          {
            tensors: [
              { data_type: 1 },
              { data_type: 0 },
            ],
          },
        ],
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
  RunSample1Request req;
  constexpr std::string_view model_path = "my_bucket/models/pcvr_models/1";
  req.add_request()->set_model_path(model_path);

  absl::StatusOr<std::unique_ptr<RunSample1Response>> resp;
  ASSERT_TRUE(app_svc->RunSample1(completed, req, resp).ok());
  completed.WaitForNotificationWithTimeout(kDefaultTimeout);

  ASSERT_TRUE(resp.ok());
  EXPECT_THAT((*resp)->response_size(), Eq(2));
  EXPECT_THAT((*resp)->response(0).model_path(),
              StrEq(absl::StrCat("foo-", model_path, "-bar")));
  EXPECT_THAT((*resp)->response(0).tensors_size(), Eq(0));
  EXPECT_THAT((*resp)->response(1).tensors_size(), Eq(2));
  EXPECT_THAT((*resp)->response(1).tensors(0).data_type(), Eq(1));
  EXPECT_THAT((*resp)->response(1).tensors(1).data_type(), Eq(0));
}

}  // namespace privacysandbox::sample::roma::AppApi::RomaSample1Test
