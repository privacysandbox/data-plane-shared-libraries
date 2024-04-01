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

#include "src/public/cpio/adapters/private_key_client/private_key_client.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/synchronization/notification.h"
#include "src/core/interface/errors.h"
#include "src/cpio/client_providers/private_key_client_provider/mock/mock_private_key_client_provider_with_overrides.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/private_key_client/mock_private_key_client_with_overrides.h"
#include "src/public/cpio/interface/private_key_client/private_key_client_interface.h"
#include "src/public/cpio/interface/private_key_client/type_def.h"
#include "src/public/cpio/proto/private_key_service/v1/private_key_service.pb.h"

using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysRequest;
using google::cmrt::sdk::private_key_service::v1::ListPrivateKeysResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::PrivateKeyClient;
using google::scp::cpio::PrivateKeyClientOptions;
using google::scp::cpio::mock::MockPrivateKeyClientWithOverrides;

namespace google::scp::cpio::test {
class PrivateKeyClientTest : public ::testing::Test {
 protected:
  PrivateKeyClientTest() {
    EXPECT_TRUE(client_.Init().ok());
    EXPECT_TRUE(client_.Run().ok());
  }

  ~PrivateKeyClientTest() { EXPECT_TRUE(client_.Stop().ok()); }

  MockPrivateKeyClientWithOverrides client_;
};

TEST_F(PrivateKeyClientTest, ListPrivateKeysSuccess) {
  EXPECT_CALL(client_.GetPrivateKeyClientProvider(), ListPrivateKeys)
      .WillOnce([=](AsyncContext<ListPrivateKeysRequest,
                                 ListPrivateKeysResponse>& context) {
        context.response = std::make_shared<ListPrivateKeysResponse>();
        context.Finish(SuccessExecutionResult());
        return absl::OkStatus();
      });

  absl::Notification finished;
  EXPECT_TRUE(client_
                  .ListPrivateKeys(ListPrivateKeysRequest(),
                                   [&](const ExecutionResult result,
                                       ListPrivateKeysResponse response) {
                                     EXPECT_THAT(result, IsSuccessful());
                                     finished.Notify();
                                   })
                  .ok());
  finished.WaitForNotification();
}

TEST_F(PrivateKeyClientTest, ListPrivateKeysFailure) {
  EXPECT_CALL(client_.GetPrivateKeyClientProvider(), ListPrivateKeys)
      .WillOnce([=](AsyncContext<ListPrivateKeysRequest,
                                 ListPrivateKeysResponse>& context) {
        context.Finish(FailureExecutionResult(SC_UNKNOWN));
        return absl::UnknownError("");
      });

  absl::Notification finished;
  EXPECT_FALSE(
      client_
          .ListPrivateKeys(ListPrivateKeysRequest(),
                           [&](const ExecutionResult result,
                               ListPrivateKeysResponse response) {
                             EXPECT_THAT(
                                 result,
                                 ResultIs(FailureExecutionResult(SC_UNKNOWN)));
                             finished.Notify();
                           })
          .ok());
  finished.WaitForNotification();
}

TEST_F(PrivateKeyClientTest, FailureToCreatePrivateKeyClientProvider) {
  client_.create_private_key_client_provider_result = absl::UnknownError("");
  EXPECT_FALSE(client_.Init().ok());
}
}  // namespace google::scp::cpio::test
