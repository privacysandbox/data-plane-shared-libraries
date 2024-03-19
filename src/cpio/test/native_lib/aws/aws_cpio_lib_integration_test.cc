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

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "absl/synchronization/blocking_counter.h"
#include "absl/synchronization/notification.h"
#include "src/core/test/utils/aws_helper/aws_helper.h"
#include "src/core/test/utils/docker_helper/docker_helper.h"
#include "src/core/utils/base64.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/blob_storage_client/test_aws_blob_storage_client.h"
#include "src/public/cpio/adapters/kms_client/test_aws_kms_client.h"
#include "src/public/cpio/adapters/metric_client/test_aws_metric_client.h"
#include "src/public/cpio/adapters/parameter_client/test_aws_parameter_client.h"
#include "src/public/cpio/test/blob_storage_client/test_aws_blob_storage_client_options.h"
#include "src/public/cpio/test/kms_client/test_aws_kms_client_options.h"
#include "src/public/cpio/test/parameter_client/test_aws_parameter_client_options.h"

using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::cmrt::sdk::metric_service::v1::MetricUnit;
using google::cmrt::sdk::metric_service::v1::PutMetricsRequest;
using google::cmrt::sdk::metric_service::v1::PutMetricsResponse;
using google::cmrt::sdk::parameter_service::v1::GetParameterRequest;
using google::cmrt::sdk::parameter_service::v1::GetParameterResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::test::CreateBucket;
using google::scp::core::test::CreateKey;
using google::scp::core::test::CreateKMSClient;
using google::scp::core::test::CreateS3Client;
using google::scp::core::test::CreateSSMClient;
using google::scp::core::test::Encrypt;
using google::scp::core::test::GetParameter;
using google::scp::core::test::PutParameter;
using google::scp::core::test::StartLocalStackContainer;
using google::scp::core::test::StopContainer;
using google::scp::core::utils::Base64Encode;
using google::scp::cpio::TestAwsBlobStorageClientOptions;
using google::scp::cpio::TestAwsKmsClient;
using google::scp::cpio::TestAwsKmsClientOptions;
using google::scp::cpio::TestAwsMetricClient;
using google::scp::cpio::TestAwsMetricClientOptions;
using google::scp::cpio::TestAwsParameterClient;
using google::scp::cpio::TestAwsParameterClientOptions;
using ::testing::StrEq;

namespace {
constexpr std::string_view kLocalHost = "http://127.0.0.1";
constexpr std::string_view kLocalstackContainerName =
    "cpio_integration_test_localstack";
// TODO(b/241857324): pick available ports randomly.
constexpr std::string_view kLocalstackPort = "8888";
constexpr std::string_view kParameterName = "test_parameter_name";
constexpr std::string_view kParameterValue = "test_parameter_value";
constexpr std::string_view kBucketName = "blob-storage-service-test-bucket";
constexpr std::string_view kBlobName = "blob_name";
constexpr std::string_view kBlobData = "some sample data";
constexpr std::string_view kPlaintext = "plaintext";
}  // namespace

namespace google::scp::cpio::test {
std::shared_ptr<PutMetricsRequest> CreatePutMetricsRequest() {
  auto request = std::make_shared<PutMetricsRequest>();
  request->set_metric_namespace("test");
  auto metric = request->add_metrics();
  metric->set_name("test_metric");
  metric->set_value("12");
  metric->set_unit(
      cmrt::sdk::metric_service::v1::MetricUnit::METRIC_UNIT_COUNT);

  auto& labels = *metric->mutable_labels();
  labels["label_key"] = "label_value";
  return request;
}

class CpioIntegrationTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    // Starts localstack
    if (StartLocalStackContainer("", kLocalstackContainerName,
                                 kLocalstackPort) != 0) {
      throw std::runtime_error("Failed to start localstack!");
    }
  }

  static void TearDownTestSuite() { StopContainer(kLocalstackContainerName); }

  void SetUp() override {
    cpio_options.log_option = LogOption::kConsoleLog;
    cpio_options.region = "us-east-1";
    cpio_options.project_id = "123456789";
    cpio_options.instance_id = "987654321";
    cpio_options.sts_endpoint_override = localstack_endpoint;
  }

  void TearDown() override {
    if (metric_client) {
      EXPECT_SUCCESS(metric_client->Stop());
    }
    if (parameter_client) {
      EXPECT_SUCCESS(parameter_client->Stop());
    }
    if (blob_storage_client) {
      EXPECT_SUCCESS(blob_storage_client->Stop());
    }
    if (kms_client) {
      EXPECT_SUCCESS(kms_client->Stop());
    }
  }

  void CreateMetricClient() {
    auto metric_client_options = std::make_shared<TestAwsMetricClientOptions>();
    metric_client_options->cloud_watch_endpoint_override =
        std::make_shared<std::string>(localstack_endpoint);
    metric_client =
        std::make_unique<TestAwsMetricClient>(metric_client_options);

    EXPECT_SUCCESS(metric_client->Init());
    EXPECT_SUCCESS(metric_client->Run());
  }

  void CreateParameterClientAndSetupData() {
    // Setup test data.
    auto ssm_client = CreateSSMClient(localstack_endpoint);
    PutParameter(ssm_client, kParameterName, kParameterValue);

    bool parameter_available = false;
    int8_t retry_count = 0;
    while (!parameter_available && retry_count < 20) {
      parameter_available = !GetParameter(ssm_client, kParameterName).empty();
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ++retry_count;
    }

    auto parameter_client_options =
        std::make_shared<TestAwsParameterClientOptions>();
    parameter_client_options->ssm_endpoint_override =
        std::make_shared<std::string>(localstack_endpoint);
    parameter_client =
        std::make_unique<TestAwsParameterClient>(parameter_client_options);

    EXPECT_SUCCESS(parameter_client->Init());
    EXPECT_SUCCESS(parameter_client->Run());
  }

  void CreateBlobStorageClientAndSetupData() {
    // Setup test data.
    auto s3_client = CreateS3Client(localstack_endpoint);
    CreateBucket(s3_client, kBucketName);

    auto blob_storage_client_options =
        std::make_shared<TestAwsBlobStorageClientOptions>();
    blob_storage_client_options->s3_endpoint_override =
        std::make_shared<std::string>(localstack_endpoint);
    blob_storage_client =
        std::make_unique<TestAwsBlobStorageClient>(blob_storage_client_options);

    EXPECT_SUCCESS(blob_storage_client->Init());
    EXPECT_SUCCESS(blob_storage_client->Run());
  }

  void CreateKmsClientAndSetupData(std::string& key_resource_name,
                                   std::string& ciphertext) {
    // Setup test data.
    auto aws_kms_client = CreateKMSClient(localstack_endpoint);
    std::string key_id;
    CreateKey(aws_kms_client, key_id, key_resource_name);
    std::string raw_ciphertext = Encrypt(aws_kms_client, key_id, kPlaintext);

    // KmsClient takes in encoded text.
    Base64Encode(raw_ciphertext, ciphertext);

    auto kms_client_options = std::make_shared<TestAwsKmsClientOptions>();
    kms_client_options->kms_endpoint_override =
        std::make_shared<std::string>(localstack_endpoint);
    kms_client = std::make_unique<TestAwsKmsClient>(kms_client_options);

    EXPECT_SUCCESS(kms_client->Init());
    EXPECT_SUCCESS(kms_client->Run());
  }

  std::string localstack_endpoint =
      std::string(kLocalHost) + ":" + std::string(kLocalstackPort);
  std::unique_ptr<TestAwsMetricClient> metric_client;
  std::unique_ptr<TestAwsParameterClient> parameter_client;
  std::unique_ptr<TestAwsBlobStorageClient> blob_storage_client;
  std::unique_ptr<TestAwsKmsClient> kms_client;
};

TEST_F(CpioIntegrationTest, MetricClientPutMetricsSuccessfully) {
  CreateMetricClient();

  absl::BlockingCounter counter(10);
  constexpr int kNumThreads = 2;
  std::vector<std::thread> threads;
  threads.reserve(kNumThreads);
  for (int i = 0; i < kNumThreads; ++i) {
    threads.emplace_back([&] {
      for (int j = 0; j < 5; ++j) {
        EXPECT_SUCCESS(metric_client->PutMetrics(
            AsyncContext<PutMetricsRequest, PutMetricsResponse>(
                CreatePutMetricsRequest(),
                [&](AsyncContext<PutMetricsRequest, PutMetricsResponse>
                        context) {
                  EXPECT_SUCCESS(context.result);
                  counter.DecrementCount();
                })));
      }
    });
  }

  // These threads complete nearly instantly because they only launch async
  // work.
  for (std::thread& t : threads) {
    t.join();
  }
  counter.Wait();
}

// GetInstanceId and GetTag cannot be tested in Localstack.
TEST_F(CpioIntegrationTest, ParameterClientGetParameterSuccessfully) {
  CreateParameterClientAndSetupData();

  absl::Notification finished;
  GetParameterRequest request;
  request.set_parameter_name(kParameterName);
  EXPECT_EQ(
      parameter_client->GetParameter(
          std::move(request),
          [&](const ExecutionResult result, GetParameterResponse response) {
            EXPECT_SUCCESS(result);
            EXPECT_EQ(response.parameter_value(), kParameterValue);
            finished.Notify();
          }),
      SuccessExecutionResult());
  ASSERT_TRUE(finished.WaitForNotificationWithTimeout(absl::Minutes(1)));
}

TEST_F(CpioIntegrationTest, BlobStorageClientPutBlobSuccessfully) {
  CreateBlobStorageClientAndSetupData();

  absl::Notification finished;
  auto request = std::make_shared<PutBlobRequest>();
  request->mutable_blob()->mutable_metadata()->set_bucket_name(kBucketName);
  request->mutable_blob()->mutable_metadata()->set_blob_name(kBlobName);
  request->mutable_blob()->set_data(kBlobData);

  auto put_blob_context = AsyncContext<PutBlobRequest, PutBlobResponse>(
      std::move(request), [&](auto& context) {
        EXPECT_SUCCESS(context.result);
        finished.Notify();
      });

  EXPECT_SUCCESS(blob_storage_client->PutBlob(put_blob_context));
  ASSERT_TRUE(finished.WaitForNotificationWithTimeout(absl::Minutes(1)));
}

TEST_F(CpioIntegrationTest, KmsClientDecryptSuccessfully) {
  std::string key_resource_name;
  std::string ciphertext;
  CreateKmsClientAndSetupData(key_resource_name, ciphertext);

  absl::Notification finished;
  auto request = std::make_shared<DecryptRequest>();
  request->set_ciphertext(ciphertext);
  request->set_kms_region("us-east-1");
  request->set_key_resource_name(key_resource_name);
  // Set a fake identity. Localstack has no authentication check.
  request->set_account_identity("arn:aws:iam::123456:role/test_create_key");

  auto decrypt_context = AsyncContext<DecryptRequest, DecryptResponse>(
      std::move(request), [&](auto& context) {
        ASSERT_SUCCESS(context.result);
        EXPECT_THAT(context.response->plaintext(), StrEq(kPlaintext));
        finished.Notify();
      });

  EXPECT_SUCCESS(kms_client->Decrypt(decrypt_context));
  ASSERT_TRUE(finished.WaitForNotificationWithTimeout(absl::Minutes(1)));
}
}  // namespace google::scp::cpio::test
