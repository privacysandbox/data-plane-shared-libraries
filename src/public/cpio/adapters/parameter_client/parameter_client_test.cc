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

#include "src/public/cpio/adapters/parameter_client/parameter_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/notification.h"
#include "src/core/interface/errors.h"
#include "src/cpio/client_providers/parameter_client_provider/mock/mock_parameter_client_provider.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/parameter_client/mock_parameter_client_with_overrides.h"
#include "src/public/cpio/interface/parameter_client/parameter_client_interface.h"
#include "src/public/cpio/interface/parameter_client/type_def.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::ParameterClient;
using google::scp::cpio::ParameterClientOptions;
using google::scp::cpio::mock::MockParameterClientWithOverrides;

namespace google::scp::cpio::test {
class ParameterClientTest : public ::testing::Test {
 protected:
  ParameterClientTest() {
    EXPECT_TRUE(client_.Init().ok());
    EXPECT_TRUE(client_.Run().ok());
  }

  ~ParameterClientTest() { EXPECT_TRUE(client_.Stop().ok()); }

  MockParameterClientWithOverrides client_;
};

TEST_F(ParameterClientTest, GetParameterSuccess) {
  EXPECT_CALL(client_.GetParameterClientProvider(), GetParameter)
      .WillOnce([=](AsyncContext<GetParameterRequest, GetParameterResponse>&
                        context) {
        context.response = std::make_shared<GetParameterResponse>();
        context.Finish(SuccessExecutionResult());
        return absl::OkStatus();
      });

  absl::Notification finished;
  EXPECT_TRUE(client_
                  .GetParameter(GetParameterRequest(),
                                [&](const ExecutionResult result,
                                    GetParameterResponse response) {
                                  EXPECT_THAT(result, IsSuccessful());
                                  finished.Notify();
                                })
                  .ok());
  finished.WaitForNotification();
}

TEST_F(ParameterClientTest, GetParameterFailure) {
  EXPECT_CALL(client_.GetParameterClientProvider(), GetParameter)
      .WillOnce([=](AsyncContext<GetParameterRequest, GetParameterResponse>&
                        context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return absl::UnknownError("");
      });

  absl::Notification finished;
  EXPECT_FALSE(
      client_
          .GetParameter(
              GetParameterRequest(),
              [&](const ExecutionResult result, GetParameterResponse response) {
                EXPECT_THAT(result,
                            ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                finished.Notify();
              })
          .ok());
  finished.WaitForNotification();
}
}  // namespace google::scp::cpio::test
