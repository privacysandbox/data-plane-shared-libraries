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
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/instance_client/mock_instance_client_with_overrides.h"
#include "src/public/cpio/core/mock_lib_cpio.h"
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
  InstanceClientTest() : client_(std::make_shared<InstanceClientOptions>()) {
    EXPECT_THAT(client_.Init(), IsSuccessful());
    EXPECT_THAT(client_.Run(), IsSuccessful());
  }

  ~InstanceClientTest() { EXPECT_THAT(client_.Stop(), IsSuccessful()); }

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
            return SuccessExecutionResult();
          });

  absl::Notification finished;
  EXPECT_THAT(client_.GetCurrentInstanceResourceName(
                  GetCurrentInstanceResourceNameRequest(),
                  [&](const ExecutionResult result,
                      GetCurrentInstanceResourceNameResponse response) {
                    EXPECT_THAT(result, IsSuccessful());
                    finished.Notify();
                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetCurrentInstanceResourceNameFailure) {
  EXPECT_CALL(client_.GetInstanceClientProvider(),
              GetCurrentInstanceResourceName)
      .WillOnce(
          [=](AsyncContext<GetCurrentInstanceResourceNameRequest,
                           GetCurrentInstanceResourceNameResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return FailureExecutionResult(SC_UNKNOWN);
          });

  absl::Notification finished;
  EXPECT_THAT(client_.GetCurrentInstanceResourceName(
                  GetCurrentInstanceResourceNameRequest(),
                  [&](const ExecutionResult result,
                      GetCurrentInstanceResourceNameResponse response) {
                    EXPECT_THAT(result,
                                ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                    finished.Notify();
                  }),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetTagsByResourceNameSuccess) {
  EXPECT_CALL(client_.GetInstanceClientProvider(), GetTagsByResourceName)
      .WillOnce([=](AsyncContext<GetTagsByResourceNameRequest,
                                 GetTagsByResourceNameResponse>& context) {
        context.response = std::make_shared<GetTagsByResourceNameResponse>();
        context.Finish(SuccessExecutionResult());
        return SuccessExecutionResult();
      });

  absl::Notification finished;
  EXPECT_THAT(client_.GetTagsByResourceName(
                  GetTagsByResourceNameRequest(),
                  [&](const ExecutionResult result,
                      GetTagsByResourceNameResponse response) {
                    EXPECT_THAT(result, IsSuccessful());
                    finished.Notify();
                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetTagsByResourceNameFailure) {
  EXPECT_CALL(client_.GetInstanceClientProvider(), GetTagsByResourceName)
      .WillOnce([=](AsyncContext<GetTagsByResourceNameRequest,
                                 GetTagsByResourceNameResponse>& context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return FailureExecutionResult(SC_UNKNOWN);
      });

  absl::Notification finished;
  EXPECT_THAT(client_.GetTagsByResourceName(
                  GetTagsByResourceNameRequest(),
                  [&](const ExecutionResult result,
                      GetTagsByResourceNameResponse response) {
                    EXPECT_THAT(result,
                                ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                    finished.Notify();
                  }),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
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
            return SuccessExecutionResult();
          });

  absl::Notification finished;
  EXPECT_THAT(client_.GetInstanceDetailsByResourceName(
                  GetInstanceDetailsByResourceNameRequest(),
                  [&](const ExecutionResult result,
                      GetInstanceDetailsByResourceNameResponse response) {
                    EXPECT_THAT(result, IsSuccessful());
                    finished.Notify();
                  }),
              IsSuccessful());
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, GetInstanceDetailsByResourceNameFailure) {
  EXPECT_CALL(client_.GetInstanceClientProvider(),
              GetInstanceDetailsByResourceName)
      .WillOnce(
          [=](AsyncContext<GetInstanceDetailsByResourceNameRequest,
                           GetInstanceDetailsByResourceNameResponse>& context) {
            context.Finish(FailureExecutionResult(SC_UNKNOWN));
            return FailureExecutionResult(SC_UNKNOWN);
          });

  absl::Notification finished;
  EXPECT_THAT(client_.GetInstanceDetailsByResourceName(
                  GetInstanceDetailsByResourceNameRequest(),
                  [&](const ExecutionResult result,
                      GetInstanceDetailsByResourceNameResponse response) {
                    EXPECT_THAT(result,
                                ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                    finished.Notify();
                  }),
              ResultIs(FailureExecutionResult(SC_UNKNOWN)));
  finished.WaitForNotification();
}

TEST_F(InstanceClientTest, FailureToCreateInstanceClientProvider) {
  auto failure = FailureExecutionResult(SC_UNKNOWN);
  client_.create_instance_client_provider_result = failure;
  EXPECT_EQ(client_.Init(), failure);
}
}  // namespace google::scp::cpio::test
