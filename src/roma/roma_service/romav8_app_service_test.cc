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

#include "src/roma/roma_service/romav8_app_service.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "absl/base/const_init.h"
#include "absl/synchronization/notification.h"
#include "src/logger/request_context_logger.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/helloworld.pb.h"
#include "src/roma/roma_service/roma_service.h"
#include "src/util/duration.h"

using ::testing::ElementsAreArray;
using ::testing::StrEq;

namespace google::scp::roma::romav8::app_api {

template <>
absl::Status Decode(const TEncoded& encoded, std::string& decoded) {
  decoded = encoded;
  return absl::OkStatus();
}

template <>
absl::StatusOr<TEncoded> Encode(const std::string& obj) {
  return obj;
}

}  // namespace google::scp::roma::romav8::app_api

namespace google::scp::roma::test {

class HelloWorldApp
    : public google::scp::roma::romav8::app_api::RomaV8AppService<> {
 public:
  using Request = std::string;
  using Response = std::string;
  static absl::StatusOr<HelloWorldApp> Create(Config config) {
    auto service = HelloWorldApp(std::move(config));
    PS_RETURN_IF_ERROR(service.Init());
    return service;
  }

  absl::Status Hello1(absl::Notification& notification, const Request& request,
                      Response& response) {
    return Execute(notification, "Hello1", request, response);
  }

  absl::Status Hello2(absl::Notification& notification, const Request& request,
                      Response& response) {
    return Execute(notification, "Hello2", request, response);
  }

 private:
  explicit HelloWorldApp(Config config)
      : RomaV8AppService(std::move(config),
                         "fully-qualified-hello-world-name") {}
};

TEST(RomaV8AppServiceTest, EncodeDecodeProtobuf) {
  ::romav8::app_api::test::HelloWorldRequest req;
  req.set_name("Foobar");

  using google::scp::roma::romav8::app_api::Decode;
  using google::scp::roma::romav8::app_api::Encode;

  const auto encoded = Encode(req);
  EXPECT_TRUE(encoded.ok());
  std::string decoded;
  EXPECT_TRUE(Decode<>(*encoded, decoded).ok());
  const auto encoded2 = Encode(decoded);
  EXPECT_TRUE(encoded2.ok());
  EXPECT_THAT(*encoded, testing::StrEq(*encoded2));
}

TEST(RomaV8AppServiceTest, HelloWorld) {
  absl::Notification load_finished;
  absl::Status load_status;
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app = HelloWorldApp::Create(std::move(config));
  EXPECT_TRUE(app.ok());

  constexpr std::string_view jscode = R"(
    var Hello1 = (input) => `Hello ${input} [Hello1]`;
    var Hello2 = function(input) {
      return "Hello world! " + input + " [Hello2]";
    }
  )";
  const std::string req = "Foobar";

  EXPECT_TRUE(app->Register(load_finished, load_status, jscode).ok());
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  std::string resp1;
  absl::Notification execute_finished1;
  EXPECT_TRUE(app->Hello1(execute_finished1, req, resp1).ok());

  std::string resp2;
  absl::Notification execute_finished2;
  EXPECT_TRUE(app->Hello2(execute_finished2, req, resp2).ok());

  execute_finished1.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(resp1, testing::StrEq("Hello Foobar [Hello1]"));

  execute_finished2.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_THAT(resp2, testing::StrEq("Hello world! Foobar [Hello2]"));
}

}  // namespace google::scp::roma::test
