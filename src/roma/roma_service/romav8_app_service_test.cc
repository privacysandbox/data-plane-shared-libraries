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

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/helloworld.pb.h"
#include "src/util/execution_token.h"

using google::scp::roma::ExecutionToken;
using ::testing::ElementsAreArray;
using ::testing::StrEq;

namespace {
constexpr std::string_view kCodeVersion = "v1";
}  // namespace

namespace google::scp::roma::romav8 {

template <>
absl::Status Decode(const std::string& encoded, std::string& decoded) {
  decoded = encoded;
  return absl::OkStatus();
}

template <>
absl::StatusOr<std::string> Encode(const std::string& obj) {
  return obj;
}

}  // namespace google::scp::roma::romav8

namespace google::scp::roma::test {

void HelloWorldFunction(FunctionBindingPayload<>& wrapper) {
  wrapper.io_proto.set_output_string(absl::StrCat(
      wrapper.metadata.at(wrapper.io_proto.input_string()), " From C++"));
}

class HelloWorldApp
    : public google::scp::roma::romav8::app_api::RomaV8AppService<> {
 public:
  using Request = std::string;
  using Response = std::string;
  using Metadata = google::scp::roma::DefaultMetadata;
  static absl::StatusOr<HelloWorldApp> Create(Config config) {
    auto service = HelloWorldApp(std::move(config));
    PS_RETURN_IF_ERROR(service.Init());
    return service;
  }

  absl::StatusOr<ExecutionToken> Hello1(
      absl::Notification& notification, const Request& request,
      absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>>& response,
      Metadata metadata = Metadata()) {
    return Execute(notification, "Hello1", request, response,
                   std::move(metadata));
  }

  absl::StatusOr<ExecutionToken> Hello1(
      absl::AnyInvocable<void(absl::StatusOr<HelloWorldApp::Response>)>
          callback,
      const Request& request, Metadata metadata = Metadata()) {
    return Execute(std::move(callback), "Hello1", request, std::move(metadata));
  }

  absl::StatusOr<ExecutionToken> Hello2(
      absl::Notification& notification, const Request& request,
      absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>>& response) {
    return Execute(notification, "Hello2", request, response);
  }

 private:
  explicit HelloWorldApp(Config config)
      : RomaV8AppService(std::move(config),
                         "fully-qualified-hello-world-name") {}
};

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

  EXPECT_TRUE(
      app->Register(jscode, kCodeVersion, load_finished, load_status).ok());
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>> resp1;
  absl::Notification execute_finished1;
  EXPECT_TRUE(app->Hello1(execute_finished1, req, resp1).ok());

  absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>> resp2;
  absl::Notification execute_finished2;
  EXPECT_TRUE(app->Hello2(execute_finished2, req, resp2).ok());

  execute_finished1.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_TRUE(resp1.ok());
  EXPECT_NE(*resp1, nullptr);
  EXPECT_THAT(**resp1, testing::StrEq("Hello Foobar [Hello1]"));

  execute_finished2.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_TRUE(resp2.ok());
  EXPECT_NE(*resp2, nullptr);
  EXPECT_THAT(**resp2, testing::StrEq("Hello world! Foobar [Hello2]"));
}

TEST(RomaV8AppServiceTest, CallbackBasedHelloWorld) {
  absl::Notification load_finished;
  absl::Status load_status;
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app = HelloWorldApp::Create(std::move(config));
  EXPECT_TRUE(app.ok());

  constexpr std::string_view jscode = R"(
    var Hello1 = (input) => `Hello ${input} [Hello1]`;
  )";
  const std::string req = "Foobar";

  EXPECT_TRUE(
      app->Register(jscode, kCodeVersion, load_finished, load_status).ok());
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  absl::StatusOr<HelloWorldApp::Response> response;
  absl::Notification execute_finished;

  auto callback = [&response, &execute_finished](
                      absl::StatusOr<HelloWorldApp::Response> resp) {
    response = std::move(resp);
    execute_finished.Notify();
  };
  EXPECT_TRUE(app->Hello1(callback, req).ok());

  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_TRUE(response.ok());
  EXPECT_THAT(*response, testing::StrEq("Hello Foobar [Hello1]"));
}

TEST(RomaV8AppServiceTest, MetadataSupportedInRomaV8AppService) {
  absl::Notification load_finished;
  absl::Status load_status;
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  config.RegisterFunctionBinding(
      std::make_unique<FunctionBindingObjectV2<>>(FunctionBindingObjectV2<>{
          .function_name = "HelloWorld",
          .function = HelloWorldFunction,
      }));
  auto app = HelloWorldApp::Create(std::move(config));
  EXPECT_TRUE(app.ok());

  constexpr std::string_view jscode = R"(
    var Hello1 = function(input) {
      return HelloWorld(input);
    }
  )";
  const std::string metadata_key = "Foobar";
  const std::string metadata_value = "Hello world!";

  EXPECT_TRUE(
      app->Register(jscode, kCodeVersion, load_finished, load_status).ok());
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>> resp;
  absl::Notification execute_finished;
  EXPECT_TRUE(app->Hello1(execute_finished, metadata_key, resp,
                          {{metadata_key, metadata_value}})
                  .ok());

  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  ASSERT_TRUE(resp.ok());
  EXPECT_NE(*resp, nullptr);
  EXPECT_THAT(**resp, testing::StrEq("Hello world! From C++"));
}

TEST(RomaV8AppServiceTest, ErrorCanBeFetchedFromAbslStatusOrResponse) {
  absl::Notification load_finished;
  absl::Status load_status;
  google::scp::roma::Config config;
  config.number_of_workers = 2;
  auto app = HelloWorldApp::Create(std::move(config));
  EXPECT_TRUE(app.ok());

  constexpr std::string_view jscode = R"(
    let x;
    var Hello1 = (input) => x.value;
  )";
  const std::string input = "";

  EXPECT_TRUE(
      app->Register(jscode, kCodeVersion, load_finished, load_status).ok());
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>> resp;
  absl::Notification execute_finished;
  EXPECT_TRUE(app->Hello1(execute_finished, input, resp).ok());

  execute_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_FALSE(resp.ok());
  EXPECT_EQ(resp.status().code(), absl::StatusCode::kInternal);
}

TEST(RomaV8AppServiceTest, CanCancelAppApiRequest) {
  absl::Notification load_finished;
  absl::Status load_status;
  google::scp::roma::Config config;
  config.number_of_workers = 1;
  auto app = HelloWorldApp::Create(std::move(config));
  EXPECT_TRUE(app.ok());

  constexpr std::string_view jscode = R"(
    var Hello1 = function(input) {
      const startTime = Date.now();
      while (Date.now() - startTime < 1000) {}
      return "Hello World";
    }
    var Hello2 = function(input) { return "Hello World" }
  )";
  const std::string input = "";

  EXPECT_TRUE(
      app->Register(jscode, kCodeVersion, load_finished, load_status).ok());
  load_finished.WaitForNotificationWithTimeout(absl::Seconds(10));

  absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>> resp1;
  absl::Notification hello1_finished;
  EXPECT_TRUE(app->Hello1(hello1_finished, input, resp1).ok());

  absl::StatusOr<std::unique_ptr<HelloWorldApp::Response>> resp2;
  absl::Notification hello2_finished;
  auto execution_token = app->Hello2(hello2_finished, input, resp2);
  EXPECT_TRUE(execution_token.ok());
  app->Cancel(*execution_token);

  hello1_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  hello2_finished.WaitForNotificationWithTimeout(absl::Seconds(10));
  EXPECT_TRUE(resp1.ok());
  EXPECT_FALSE(resp2.ok());
  EXPECT_EQ(resp2.status().code(), absl::StatusCode::kCancelled);
}

}  // namespace google::scp::roma::test
