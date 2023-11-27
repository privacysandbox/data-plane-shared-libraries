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

#include "cpio/client_providers/parameter_client_provider/src/aws/aws_parameter_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <aws/core/Aws.h>
#include <aws/core/utils/Outcome.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/SSMErrors.h>
#include <aws/ssm/model/GetParameterRequest.h>

#include "absl/synchronization/notification.h"
#include "core/async_executor/mock/mock_async_executor.h"
#include "core/interface/async_context.h"
#include "cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "cpio/client_providers/parameter_client_provider/mock/aws/mock_ssm_client.h"
#include "cpio/common/src/aws/error_codes.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/parameter_service/v1/parameter_service.pb.h"
#include "public/cpio/test/global_cpio/test_cpio_options.h"
#include "public/cpio/test/global_cpio/test_lib_cpio.h"

using Aws::Client::AWSError;
using Aws::SSM::SSMErrors;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME;
using google::scp::core::errors::
    SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND;
using google::scp::core::test::ResultIs;
using google::scp::cpio::TestCpioOptions;
using google::scp::cpio::TestLibCpio;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockSSMClient;
using testing::NiceMock;
using testing::Return;
using testing::StrEq;

namespace {
constexpr char kResourceNameMock[] =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr char kParameterName[] = "name";
constexpr char kParameterValue[] = "value";
constexpr char kRegionMock[] = "us-test-east-1";
}  // namespace

namespace google::scp::cpio::client_providers::test {
class MockSSMClientFactory : public SSMClientFactory {
 public:
  MOCK_METHOD(
      std::shared_ptr<Aws::SSM::SSMClient>, CreateSSMClient,
      (Aws::Client::ClientConfiguration & client_config,
       const std::shared_ptr<core::AsyncExecutorInterface>& io_async_executor),
      (noexcept, override));
};

class AwsParameterClientProviderTest : public ::testing::Test {
 protected:
  void SetUp() override {
    cpio_options_.log_option = LogOption::kConsoleLog;
    cpio_options_.region = kRegionMock;
    EXPECT_SUCCESS(TestLibCpio::InitCpio(cpio_options_));

    auto io_async_executor_mock = std::make_shared<MockAsyncExecutor>();

    auto instance_client_mock = std::make_shared<MockInstanceClientProvider>();
    instance_client_mock->instance_resource_name = kResourceNameMock;

    mock_ssm_client_ = std::make_shared<MockSSMClient>();
    auto ssm_client_factory_mock =
        std::make_shared<NiceMock<MockSSMClientFactory>>();
    ON_CALL(*ssm_client_factory_mock, CreateSSMClient)
        .WillByDefault(Return(mock_ssm_client_));

    client_ = std::make_unique<AwsParameterClientProvider>(
        std::make_shared<ParameterClientOptions>(), instance_client_mock,
        io_async_executor_mock, ssm_client_factory_mock);

    EXPECT_SUCCESS(client_->Init());
    EXPECT_SUCCESS(client_->Run());
  }

  void MockParameter() {
    // Mocks Aws::SSM::Model::GetParameterRequest.
    Aws::SSM::Model::GetParameterRequest get_parameter_request;
    get_parameter_request.SetName(kParameterName);
    mock_ssm_client_->get_parameter_request_mock = get_parameter_request;

    // Mocks success Aws::SSM::Model::GetParameterOutcome.
    Aws::SSM::Model::GetParameterResult result;
    Aws::SSM::Model::Parameter parameter;
    parameter.SetName(kParameterName);
    parameter.SetValue(kParameterValue);
    result.SetParameter(parameter);
    Aws::SSM::Model::GetParameterOutcome get_parameter_outcome(result);
    mock_ssm_client_->get_parameter_outcome_mock = get_parameter_outcome;
  }

  void TearDown() override {
    EXPECT_SUCCESS(client_->Stop());
    EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(cpio_options_));
  }

  std::shared_ptr<MockSSMClient> mock_ssm_client_;
  std::unique_ptr<AwsParameterClientProvider> client_;
  TestCpioOptions cpio_options_;
};

TEST_F(AwsParameterClientProviderTest, FailedToFetchParameter) {
  MockParameter();
  AWSError<SSMErrors> error(SSMErrors::INTERNAL_FAILURE,
                            /* isRetryable=*/false);
  Aws::SSM::Model::GetParameterOutcome outcome(error);
  mock_ssm_client_->get_parameter_outcome_mock = outcome;

  absl::Notification done;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);

  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
        done.Notify();
      });
  EXPECT_SUCCESS(client_->GetParameter(context));
  done.WaitForNotification();
}

TEST_F(AwsParameterClientProviderTest, InvalidParameterName) {
  absl::Notification done;
  auto request = std::make_shared<GetParameterRequest>();
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(
            context.result,
            ResultIs(FailureExecutionResult(
                SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
        done.Notify();
      });
  EXPECT_THAT(client_->GetParameter(context),
              ResultIs(FailureExecutionResult(
                  SC_AWS_PARAMETER_CLIENT_PROVIDER_INVALID_PARAMETER_NAME)));
  done.WaitForNotification();
}

TEST_F(AwsParameterClientProviderTest, ParameterNotFound) {
  const std::string invalid_parameter_name("invalid_parameter");

  // Mocks Aws::SSM::Model::GetParameterRequest with invalid_parameter.
  Aws::SSM::Model::GetParameterRequest get_parameter_request;
  get_parameter_request.SetName(invalid_parameter_name);
  mock_ssm_client_->get_parameter_request_mock = get_parameter_request;
  // Mocks Aws::SSM::Model::GetParameterOutcome with error parameter not found
  // AWS error.
  AWSError<SSMErrors> error(SSMErrors::PARAMETER_NOT_FOUND,
                            /* isRetryable=*/false);
  Aws::SSM::Model::GetParameterOutcome outcome(error);
  mock_ssm_client_->get_parameter_outcome_mock = outcome;

  absl::Notification done;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(invalid_parameter_name);
  AsyncContext<GetParameterRequest, GetParameterResponse> context(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_THAT(context.result,
                    ResultIs(FailureExecutionResult(
                        SC_AWS_PARAMETER_CLIENT_PROVIDER_PARAMETER_NOT_FOUND)));
        done.Notify();
      });
  EXPECT_SUCCESS(client_->GetParameter(context));
  done.WaitForNotification();
}

TEST_F(AwsParameterClientProviderTest, SucceedToFetchParameter) {
  MockParameter();

  absl::Notification done;
  auto request = std::make_shared<GetParameterRequest>();
  request->set_parameter_name(kParameterName);
  AsyncContext<GetParameterRequest, GetParameterResponse> context1(
      std::move(request),
      [&](AsyncContext<GetParameterRequest, GetParameterResponse>& context) {
        EXPECT_SUCCESS(context.result);
        EXPECT_THAT(context.response->parameter_value(),
                    StrEq(kParameterValue));
        done.Notify();
      });
  EXPECT_SUCCESS(client_->GetParameter(context1));
  done.WaitForNotification();
}

TEST(AwsParameterClientProviderTestII, FailedToFetchRegion) {
  // Set up AWS Parameter Client with different CPIO options.
  // Set up everything again to not use SetUp().
  TestCpioOptions cpio_options_no_region;
  EXPECT_SUCCESS(TestLibCpio::InitCpio(cpio_options_no_region));

  auto io_async_executor_mock = std::make_shared<MockAsyncExecutor>();

  auto instance_client_mock = std::make_shared<MockInstanceClientProvider>();
  instance_client_mock->instance_resource_name = kResourceNameMock;

  auto mock_ssm_client_ = std::make_shared<MockSSMClient>();
  auto ssm_client_factory_mock =
      std::make_shared<NiceMock<MockSSMClientFactory>>();
  ON_CALL(*ssm_client_factory_mock, CreateSSMClient)
      .WillByDefault(Return(mock_ssm_client_));

  auto client_ = std::make_unique<AwsParameterClientProvider>(
      std::make_shared<ParameterClientOptions>(), instance_client_mock,
      io_async_executor_mock, ssm_client_factory_mock);

  auto failure = FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR);
  instance_client_mock->get_instance_resource_name_mock = failure;

  EXPECT_THAT(client_->Init(), ResultIs(failure));
  EXPECT_SUCCESS(client_->Run());

  EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(cpio_options_no_region));
}

}  // namespace google::scp::cpio::client_providers::test
