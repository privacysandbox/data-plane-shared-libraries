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

#include "src/cpio/client_providers/queue_client_provider/aws/aws_queue_client_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <optional>

#include <aws/core/Aws.h>
#include <aws/sqs/SQSClient.h>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/instance_client_provider/aws/aws_instance_client_provider.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/client_providers/queue_client_provider/aws/error_codes.h"
#include "src/cpio/client_providers/queue_client_provider/mock/aws/mock_sqs_client.h"
#include "src/cpio/common/aws/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/proto/queue_service/v1/queue_service.pb.h"

namespace google::scp::cpio::client_providers::test {
namespace {

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::SQS::SQSClient;
using Aws::SQS::SQSErrors;
using Aws::SQS::Model::GetQueueUrlOutcome;
using Aws::SQS::Model::GetQueueUrlResult;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutRequest;
using google::cmrt::sdk::queue_service::v1::
    UpdateMessageVisibilityTimeoutResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionStatus;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INVALID_CREDENTIALS;
using google::scp::core::errors::SC_AWS_INVALID_REQUEST;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_RECEIPT_INFO;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_INVALID_VISIBILITY_TIMEOUT;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGE_NOT_IN_FLIGHT;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_MESSAGES_NUMBER_EXCEEDED;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_CLIENT_OPTIONS_REQUIRED;
using google::scp::core::errors::
    SC_AWS_QUEUE_CLIENT_PROVIDER_QUEUE_NAME_REQUIRED;
using google::scp::core::errors::SC_AWS_REQUEST_LIMIT_REACHED;
using google::scp::core::errors::SC_AWS_SERVICE_UNAVAILABLE;
using google::scp::core::errors::SC_AWS_VALIDATION_FAILED;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::AwsSqsClientFactory;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockSqsClient;
using ::testing::_;
using ::testing::ByMove;
using ::testing::Eq;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

constexpr std::string_view kResourceNameMock =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
constexpr std::string_view kQueueName = "queue name";
constexpr std::string_view kQueueUrl = "queue url";
constexpr std::string_view kMessageBody = "message body";
constexpr std::string_view kMessageId = "message id";
constexpr std::string_view kReceiptInfo = "receipt info";
constexpr std::string_view kInvalidReceiptInfo = "";
constexpr uint8_t kDefaultMaxNumberOfMessagesReceived = 1;
constexpr uint8_t kDefaultMaxWaitTimeSeconds = 0;
constexpr uint16_t kVisibilityTimeoutSeconds = 10;
constexpr uint16_t kInvalidVisibilityTimeoutSeconds = 50000;

class MockAwsSqsClientFactory : public AwsSqsClientFactory {
 public:
  MOCK_METHOD(std::shared_ptr<SQSClient>, CreateSqsClient,
              (const ClientConfiguration& client_config), (noexcept, override));
};

class AwsQueueClientProviderTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    SDKOptions options;
    InitAPI(options);
  }

  static void TearDownTestSuite() {
    SDKOptions options;
    ShutdownAPI(options);
  }

  AwsQueueClientProviderTest() {
    queue_client_options_.queue_name = kQueueName;

    mock_instance_client_.instance_resource_name = kResourceNameMock;
    mock_sqs_client_ = std::make_shared<NiceMock<MockSqsClient>>();

    GetQueueUrlResult get_queue_url_result;
    get_queue_url_result.SetQueueUrl(std::string{kQueueUrl});
    GetQueueUrlOutcome get_queue_url_outcome(std::move(get_queue_url_result));
    ON_CALL(*mock_sqs_client_, GetQueueUrl)
        .WillByDefault(Return(get_queue_url_outcome));

    mock_sqs_client_factory_ =
        std::make_shared<NiceMock<MockAwsSqsClientFactory>>();
    ON_CALL(*mock_sqs_client_factory_, CreateSqsClient)
        .WillByDefault(Return(mock_sqs_client_));

    update_message_visibility_timeout_context_.request =
        std::make_shared<UpdateMessageVisibilityTimeoutRequest>();
    update_message_visibility_timeout_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    queue_client_provider_.emplace(
        queue_client_options_, &mock_instance_client_, &cpu_async_executor_,
        &io_async_executor_, mock_sqs_client_factory_);
  }

  QueueClientOptions queue_client_options_;
  MockAsyncExecutor cpu_async_executor_;
  MockAsyncExecutor io_async_executor_;
  MockInstanceClientProvider mock_instance_client_;
  std::shared_ptr<MockSqsClient> mock_sqs_client_;
  std::shared_ptr<MockAwsSqsClientFactory> mock_sqs_client_factory_;
  std::optional<AwsQueueClientProvider> queue_client_provider_;

  AsyncContext<UpdateMessageVisibilityTimeoutRequest,
               UpdateMessageVisibilityTimeoutResponse>
      update_message_visibility_timeout_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  absl::Mutex finish_called_mu_;
  bool finish_called_ ABSL_GUARDED_BY(finish_called_mu_) = false;
};

TEST_F(AwsQueueClientProviderTest, RunWithEmptyQueueName) {
  queue_client_options_.queue_name = "";
  MockAsyncExecutor cpu_async_executor;
  MockAsyncExecutor io_async_executor;
  AwsQueueClientProvider client(queue_client_options_, &mock_instance_client_,
                                &cpu_async_executor, &io_async_executor,
                                mock_sqs_client_factory_);

  EXPECT_FALSE(client.Init().ok());
}

TEST_F(AwsQueueClientProviderTest, RunWithCreateClientConfigurationFailed) {
  mock_instance_client_.get_instance_resource_name_mock =
      absl::UnknownError("");
  MockAsyncExecutor cpu_async_executor;
  MockAsyncExecutor io_async_executor;
  AwsQueueClientProvider client(queue_client_options_, &mock_instance_client_,
                                &cpu_async_executor, &io_async_executor,
                                mock_sqs_client_factory_);

  EXPECT_FALSE(client.Init().ok());
}

TEST_F(AwsQueueClientProviderTest, RunWithGetQueueUrlFailed) {
  AWSError<SQSErrors> get_queue_url_error(SQSErrors::SERVICE_UNAVAILABLE,
                                          false);
  GetQueueUrlOutcome get_queue_url_outcome(get_queue_url_error);
  EXPECT_CALL(*mock_sqs_client_, GetQueueUrl)
      .WillOnce(Return(get_queue_url_outcome));

  EXPECT_FALSE(queue_client_provider_->Init().ok());
}

MATCHER_P(HasGetQueueUrlRequestParams, queue_name, "") {
  return ExplainMatchResult(Eq(queue_name), arg.GetQueueName(),
                            result_listener);
}

TEST_F(AwsQueueClientProviderTest, RunSuccessWithExistingQueue) {
  GetQueueUrlResult get_queue_url_result;
  get_queue_url_result.SetQueueUrl(std::string{kQueueUrl});
  GetQueueUrlOutcome get_queue_url_outcome(std::move(get_queue_url_result));
  EXPECT_CALL(*mock_sqs_client_,
              GetQueueUrl(HasGetQueueUrlRequestParams(kQueueName)))
      .WillOnce(Return(get_queue_url_outcome));

  EXPECT_TRUE(queue_client_provider_->Init().ok());
}

TEST_F(AwsQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutWithInvalidReceiptInfo) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kInvalidReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kInvalidVisibilityTimeoutSeconds);

  EXPECT_FALSE(queue_client_provider_
                   ->UpdateMessageVisibilityTimeout(
                       update_message_visibility_timeout_context_)
                   .ok());
  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(AwsQueueClientProviderTest,
       UpdateMessageVisibilityTimeoutWithInvalidExpirationTime) {
  EXPECT_TRUE(queue_client_provider_->Init().ok());

  update_message_visibility_timeout_context_.request->set_receipt_info(
      kReceiptInfo);
  update_message_visibility_timeout_context_.request
      ->mutable_message_visibility_timeout()
      ->set_seconds(kInvalidVisibilityTimeoutSeconds);

  EXPECT_FALSE(queue_client_provider_
                   ->UpdateMessageVisibilityTimeout(
                       update_message_visibility_timeout_context_)
                   .ok());
  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

}  // namespace
}  // namespace google::scp::cpio::client_providers::test
