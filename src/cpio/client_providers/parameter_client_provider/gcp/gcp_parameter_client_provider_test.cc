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

#include <gtest/gtest.h>

#include <memory>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "google/cloud/secretmanager/mocks/mock_secret_manager_connection.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/client_providers/parameter_client_provider/gcp/error_codes.h"
#include "src/cpio/client_providers/parameter_client_provider/mock/gcp/mock_gcp_parameter_client_provider_with_overrides.h"
#include "src/cpio/common/gcp/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/parameter_service/v1/parameter_service.pb.h"

namespace google::scp::cpio::test {
namespace {

using google::cloud::Status;
using google::cloud::StatusCode;
using google::cloud::secretmanager::SecretManagerServiceClient;
using google::cloud::secretmanager::v1::AccessSecretVersionRequest;
using google::cloud::secretmanager::v1::AccessSecretVersionResponse;
using google::cloud::secretmanager_mocks::MockSecretManagerServiceConnection;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncOperation;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_GCP_INVALID_ARGUMENT;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_CREATE_SM_CLIENT_FAILURE;
using google::scp::core::errors::
    SC_GCP_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::mock::
    MockGcpParameterClientProviderOverrides;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::NiceMock;
using testing::Return;

constexpr std::string_view kInstanceResourceName =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr std::string_view kParameterNameMock = "parameter-name-test";
constexpr std::string_view kValueMock = "value";
constexpr std::string_view kProjectIdValueMock = "123456789";

class GcpParameterClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    instance_client_mock_.instance_resource_name = kInstanceResourceName;

    client_.emplace(&async_executor_mock_, &io_async_executor_mock_,
                    &instance_client_mock_);

    connection_ =
        std::make_shared<NiceMock<MockSecretManagerServiceConnection>>();
    client_->secret_manager_mock =
        std::make_shared<SecretManagerServiceClient>(connection_);
    EXPECT_TRUE(client_->Init().ok());
  }

  std::string GetSecretName(
      std::string_view parameter_name = kParameterNameMock) {
    auto secret_name =
        absl::StrCat("projects/", kProjectIdValueMock, "/secrets/",
                     parameter_name, "/versions/latest");

    return secret_name;
  }

  MockAsyncExecutor async_executor_mock_;
  MockAsyncExecutor io_async_executor_mock_;
  MockInstanceClientProvider instance_client_mock_;
  std::shared_ptr<MockSecretManagerServiceConnection> connection_;
  std::optional<MockGcpParameterClientProviderOverrides> client_;
};

MATCHER_P(RequestHasName, secret_name, "") {
  return ExplainMatchResult(Eq(secret_name), arg.name(), result_listener);
}

TEST_F(GcpParameterClientProviderTest, SucceedToFetchParameter) {
  auto secret_name_mock = GetSecretName();

  AccessSecretVersionResponse response;
  response.mutable_payload()->set_data(kValueMock);

  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(response));

  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_EQ(context.response->parameter_value(), kValueMock);
        condition.Notify();
      });

  EXPECT_TRUE(client_->GetParameter(context).ok());
  condition.WaitForNotification();
}

TEST_F(GcpParameterClientProviderTest, FailedToFetchParameterErrorNotFound) {
  auto secret_name_mock = GetSecretName();
  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(Status(StatusCode::kNotFound, "Not Found")));

  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_NOT_FOUND)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->GetParameter(context).ok());
  condition.WaitForNotification();
}

TEST_F(GcpParameterClientProviderTest, FailedWithInvalidParameterName) {
  auto request = std::make_shared<GetParameterRequest>();
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {});
  EXPECT_FALSE(client_->GetParameter(context).ok());
}

TEST_F(GcpParameterClientProviderTest,
       FailedToFetchParameterErrorInvalidArgument) {
  auto secret_name_mock = GetSecretName();
  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(Status(StatusCode::kInvalidArgument, "")));

  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_INVALID_ARGUMENT)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->GetParameter(context).ok());
  condition.WaitForNotification();
}

TEST_F(GcpParameterClientProviderTest, FailedToFetchParameterErrorUnknown) {
  auto secret_name_mock = GetSecretName();
  EXPECT_CALL(*connection_,
              AccessSecretVersion(RequestHasName(secret_name_mock)))
      .WillOnce(Return(Status(StatusCode::kUnknown, "")));

  absl::Notification condition;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterNameMock);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));
        condition.Notify();
      });

  EXPECT_TRUE(client_->GetParameter(context).ok());
  condition.WaitForNotification();
}

TEST(GcpParameterClientProviderTestII, InitFailedToFetchProjectId) {
  MockAsyncExecutor async_executor_mock;
  MockAsyncExecutor io_async_executor_mock;
  MockInstanceClientProvider instance_client_mock;
  instance_client_mock.get_instance_resource_name_mock = absl::UnknownError("");

  auto connection =
      std::make_shared<NiceMock<MockSecretManagerServiceConnection>>();
  auto mock_sm_client =
      std::make_unique<SecretManagerServiceClient>(connection);

  MockGcpParameterClientProviderOverrides client(
      &async_executor_mock, &io_async_executor_mock, &instance_client_mock);
  EXPECT_FALSE(client.Init().ok());
}

TEST(GcpParameterClientProviderTestII, InitFailedToGetSMClient) {
  MockAsyncExecutor async_executor_mock;
  MockAsyncExecutor io_async_executor_mock;
  MockInstanceClientProvider instance_client_mock;
  instance_client_mock.instance_resource_name = kInstanceResourceName;

  MockGcpParameterClientProviderOverrides client(
      &async_executor_mock, &io_async_executor_mock, &instance_client_mock);
  EXPECT_FALSE(client.Init().ok());
}

}  // namespace
}  // namespace google::scp::cpio::test
