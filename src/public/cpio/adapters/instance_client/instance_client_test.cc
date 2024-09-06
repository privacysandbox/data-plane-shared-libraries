// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/public/cpio/adapters/instance_client/instance_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "absl/synchronization/notification.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/instance_client/mock_instance_client_with_overrides.h"
#include "src/public/cpio/interface/instance_client/instance_client_interface.h"
#include "src/public/cpio/proto/instance_service/v1/instance_service.pb.h"

using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetCurrentInstanceResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::
    GetInstanceDetailsByResourceNameResponse;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameRequest;
using google::cmrt::sdk::instance_service::v1::GetTagsByResourceNameResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::mock::MockInstanceClientWithOverrides;

namespace google::scp::cpio::test {
class InstanceClientTest : public ::testing::Test {
 protected:
  InstanceClientTest() {
    EXPECT_TRUE(client_.Init().ok());
    EXPECT_TRUE(client_.Run().ok());
  }

  ~InstanceClientTest() { EXPECT_TRUE(client_.Stop().ok()); }

  MockInstanceClientWithOverrides client_;
};

TEST_F(InstanceClientTest, GetCurrentInstanceResourceNameSuccess) {
  EXPECT_CALL(client_.GetInstanceClientProvider(),
              GetCurrentInstanceResourceName)
      .WillOnce(
          [=](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            context.response =
                std::make_shared<GetCurrentInstanceResourceNameResponse>();
            context.Finish(SuccessExecutionResult());
            return absl::OkStatus();
          });

  absl::Notification finished;
  EXPECT_TRUE(client_
                  .GetCurrentInstanceResourceName(
                      GetCurrentInstanceResourceNameRequest(),
                      [&](const ExecutionResult result,
                          GetCurrentInstanceResourceNameResponse response) {
                        EXPECT_THAT(result, IsSuccessful());
                        finished.Notify();
                      })
                  .ok());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetCurrentInstanceResourceNameFailure) {
  EXPECT_CALL(client_.GetInstanceClientProvider(),
              GetCurrentInstanceResourceName)
      .WillOnce(
          [=](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return absl::UnknownError("");
          });

  absl::Notification finished;
  EXPECT_FALSE(client_
                   .GetCurrentInstanceResourceName(
                       GetCurrentInstanceResourceNameRequest(),
                       [&](const ExecutionResult result,
                           GetCurrentInstanceResourceNameResponse response) {
                         EXPECT_THAT(
                             result,
                             ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                         finished.Notify();
                       })
                   .ok());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetTagsByResourceNameSuccess) {
  EXPECT_CALL(client_.GetInstanceClientProvider(), GetTagsByResourceName)
      .WillOnce([=](AsyncContext<GetTagsByResourceNameRequest,
                                 GetTagsByResourceNameResponse>& context) {
        context.response = std::make_shared<GetTagsByResourceNameResponse>();
        context.Finish(SuccessExecutionResult());
        return absl::OkStatus();
      });

  absl::Notification finished;
  EXPECT_TRUE(
      client_
          .GetTagsByResourceName(GetTagsByResourceNameRequest(),
                                 [&](const ExecutionResult result,
                                     GetTagsByResourceNameResponse response) {
                                   EXPECT_THAT(result, IsSuccessful());
                                   finished.Notify();
                                 })
          .ok());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetTagsByResourceNameFailure) {
  EXPECT_CALL(client_.GetInstanceClientProvider(), GetTagsByResourceName)
      .WillOnce([=](AsyncContext<GetTagsByResourceNameRequest,
                                 GetTagsByResourceNameResponse>& context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return absl::UnknownError("");
      });

  absl::Notification finished;
  EXPECT_FALSE(client_
                   .GetTagsByResourceName(
                       GetTagsByResourceNameRequest(),
                       [&](const ExecutionResult result,
                           GetTagsByResourceNameResponse response) {
                         EXPECT_THAT(
                             result,
                             ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                         finished.Notify();
                       })
                   .ok());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetInstanceDetailsByResourceNameSuccess) {
  EXPECT_CALL(client_.GetInstanceClientProvider(),
              GetInstanceDetailsByResourceName)
      .WillOnce(
          [=](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            context.response =
                std::make_shared<GetInstanceDetailsByResourceNameResponse>();
            context.Finish(SuccessExecutionResult());
            return absl::OkStatus();
          });

  absl::Notification finished;
  EXPECT_TRUE(client_
                  .GetInstanceDetailsByResourceName(
                      GetInstanceDetailsByResourceNameRequest(),
                      [&](const ExecutionResult result,
                          GetInstanceDetailsByResourceNameResponse response) {
                        EXPECT_THAT(result, IsSuccessful());
                        finished.Notify();
                      })
                  .ok());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetInstanceDetailsByResourceNameFailure) {
  EXPECT_CALL(client_.GetInstanceClientProvider(),
              GetInstanceDetailsByResourceName)
      .WillOnce(
          [=](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return absl::UnknownError("");
          });

  absl::Notification finished;
  EXPECT_FALSE(client_
                   .GetInstanceDetailsByResourceName(
                       GetInstanceDetailsByResourceNameRequest(),
                       [&](const ExecutionResult result,
                           GetInstanceDetailsByResourceNameResponse response) {
                         EXPECT_THAT(
                             result,
                             ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                         finished.Notify();
                       })
                   .ok());
  finished.WaitForNotification();
}
}  // namespace google::scp::cpio::test
