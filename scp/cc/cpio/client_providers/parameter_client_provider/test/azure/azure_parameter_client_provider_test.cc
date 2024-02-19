// Portions Copyright (c) Microsoft Corporation
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

#include <gtest/gtest.h>

#include <memory>
#include <stdlib.h>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/parameter_client_provider/src/azure/azure_parameter_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/src/azure/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

using google::scp::core::AsyncContext;
using google::scp::core::AsyncOperation;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::cpio::client_providers::AzureParameterClientProvider;
using testing::Eq;
using testing::ExplainMatchResult;
using google::scp::core::errors::SC_AZURE_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::core::errors::
    SC_AZURE_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;

namespace {
constexpr char kTestParameterName[] = "TEST_PARAM";
constexpr char kTestValue[] = "test-param-value";
constexpr char kParameterRequestPrefix[] = "azure_operator-azure_environment-";
}  // namespace

namespace google::scp::cpio::test {
class AzureParameterClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    client_ = std::make_unique<AzureParameterClientProvider>();

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());
    SetParameter(kTestParameterName, kTestValue);
  }

  void TearDown() override {
    EXPECT_SUCCESS(client_->Stop());
  }

  void SetParameter(
      const std::string& parameter_name, const std::string& parameter_value) {
    EXPECT_EQ(setenv(parameter_name.c_str(), parameter_value.c_str(), 1), 0);
  }

  std::unique_ptr<AzureParameterClientProvider> client_;
};

TEST_F(AzureParameterClientProviderTest, SucceedToFetchParameter) {
  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  const std::string parameter_with_prefix = std::string(kParameterRequestPrefix) + std::string(kTestParameterName);
  request->set_parameter_name(parameter_with_prefix);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->parameter_value(), kTestValue);
        condition.Notify();
      });

  EXPECT_SUCCESS(client_->GetParameter(context));
  condition.WaitForNotification();
}

TEST_F(AzureParameterClientProviderTest, FailedToFetchParameterErrorNotFound) {
  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  const std::string parameter_with_prefix = std::string(kParameterRequestPrefix) + std::string("DO_NOT_EXIST");
  request->set_parameter_name(parameter_with_prefix);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {});

  EXPECT_THAT(client_->GetParameter(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND)));
}

TEST_F(AzureParameterClientProviderTest, FailedWithInvalidParameterName) {
  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kTestParameterName/*No correct prefix*/);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {});

  EXPECT_THAT(client_->GetParameter(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
}

TEST_F(AzureParameterClientProviderTest,
       FailedToFetchParameterEmptyInput) {
        absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name("");
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {});

  EXPECT_THAT(client_->GetParameter(context),
              ResultIs(FailureExecutionResult(
                  SC_AZURE_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
}
}  // namespace google::scp::cpio::test