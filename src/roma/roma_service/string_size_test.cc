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

#include <chrono>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using ::google::scp::roma::FunctionBindingPayload;
using ::google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;
using ::testing::HasSubstr;
using ::testing::IsEmpty;
using ::testing::StrEq;

namespace google::scp::roma::test {
namespace {

class StringSizeTest : public testing::TestWithParam<size_t> {};

constexpr size_t kStringSizes[] = {
    100'000,
    2'000'000,
    10'000'000,
    100'000'000,
};

INSTANTIATE_TEST_SUITE_P(StringSizeTestSuite, StringSizeTest,
                         testing::ValuesIn(kStringSizes),
                         testing::PrintToStringParamName());

TEST_P(StringSizeTest, LoadCodeObj) {
  Config config;
  config.number_of_workers = 1;
  RomaService<> roma_service(std::move(config));
  ASSERT_TRUE(roma_service.Init().ok());

  std::string result;
  absl::Status response_status;

  const size_t strsize = GetParam();
  auto code_obj = std::make_unique<CodeObject>(CodeObject{
      .id = "foo",
      .version_string = "v1",
      .js = absl::StrCat("const str = `", std::string(strsize, '.'), "`;",
                         R"JSCODE(
    function Handler() {
      return `some string of length: ${str.length}`;
    })JSCODE"),
  });

  absl::Notification load_finished;
  ASSERT_TRUE(roma_service
                  .LoadCodeObj(std::move(code_obj),
                               [&](absl::StatusOr<ResponseObject> resp) {
                                 response_status = resp.status();
                                 load_finished.Notify();
                               })
                  .ok());
  ASSERT_TRUE(load_finished.WaitForNotificationWithTimeout(absl::Seconds(40)));
  ASSERT_TRUE(response_status.ok());

  auto execution_obj =
      std::make_unique<InvocationStrRequest<>>(InvocationStrRequest<>{
          .id = "foo",
          .version_string = "v1",
          .handler_name = "Handler",
      });

  absl::Notification execute_finished;
  EXPECT_TRUE(roma_service
                  .Execute(std::move(execution_obj),
                           [&execute_finished, &response_status,
                            &result](absl::StatusOr<ResponseObject> resp) {
                             response_status = resp.status();
                             result = std::move(resp->resp);
                             execute_finished.Notify();
                           })
                  .ok());
  ASSERT_TRUE(
      execute_finished.WaitForNotificationWithTimeout(absl::Seconds(40)));
  ASSERT_TRUE(response_status.ok()) << response_status;
  EXPECT_THAT(result,
              StrEq(absl::StrCat("\"some string of length: ", strsize, "\"")));
  ASSERT_TRUE(roma_service.Stop().ok());
}

}  // namespace
}  // namespace google::scp::roma::test
