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

#include "src/cpio/client_providers/blob_storage_client_provider/aws/aws_blob_storage_client_provider.h"

#include <gtest/gtest.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <aws/s3/S3Client.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/Object.h>
#include <aws/s3/model/PutObjectRequest.h>

#include "absl/base/thread_annotations.h"
#include "absl/synchronization/mutex.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/cpio/client_providers/blob_storage_client_provider/aws/mock_s3_client.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/cpio/common/aws/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using Aws::InitAPI;
using Aws::SDKOptions;
using Aws::ShutdownAPI;
using Aws::StringStream;
using Aws::Client::AWSError;
using Aws::Client::ClientConfiguration;
using Aws::S3::S3Errors;
using Aws::S3::Model::GetObjectOutcome;
using Aws::S3::Model::GetObjectRequest;
using Aws::S3::Model::GetObjectResult;
using Aws::S3::Model::Object;
using Aws::S3::Model::PutObjectOutcome;
using Aws::S3::Model::PutObjectRequest;
using Aws::S3::Model::PutObjectResult;
using google::cmrt::sdk::blob_storage_service::v1::Blob;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_AWS_INTERNAL_SERVICE_ERROR;
using google::scp::core::test::ResultIs;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using google::scp::cpio::client_providers::mock::MockS3Client;
using ::testing::_;
using testing::ByMove;
using ::testing::Eq;
using ::testing::ExplainMatchResult;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::StrEq;

namespace {
constexpr std::string_view kResourceNameMock =
    "arn:aws:ec2:us-east-1:123456789012:instance/i-0e9801d129EXAMPLE";
}

namespace google::scp::cpio::client_providers {

class MockAwsS3Factory : public AwsS3Factory {
 public:
  MOCK_METHOD(core::ExecutionResultOr<std::shared_ptr<Aws::S3::S3Client>>,
              CreateClient,
              (ClientConfiguration, core::AsyncExecutorInterface*),
              (noexcept, override));
};

class AwsBlobStorageClientProviderTest : public ::testing::Test {
 protected:
  AwsBlobStorageClientProviderTest() {
    auto s3_factory = std::make_unique<NiceMock<MockAwsS3Factory>>();
    s3_factory_ = s3_factory.get();
    provider_.emplace(BlobStorageClientOptions(), &instance_client_,
                      &cpu_async_executor_, &io_async_executor_,
                      std::move(s3_factory));
    InitAPI(options_);
    instance_client_.instance_resource_name = kResourceNameMock;
    s3_client_ = std::make_shared<NiceMock<MockS3Client>>();

    ON_CALL(*s3_factory_, CreateClient).WillByDefault(Return(s3_client_));

    get_blob_context_.request = std::make_shared<GetBlobRequest>();
    get_blob_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    put_blob_context_.request = std::make_shared<PutBlobRequest>();
    put_blob_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    EXPECT_TRUE(provider_->Init().ok());
  }

  ~AwsBlobStorageClientProviderTest() { ShutdownAPI(options_); }

  MockInstanceClientProvider instance_client_;
  MockAsyncExecutor cpu_async_executor_;
  MockAsyncExecutor io_async_executor_;
  MockAwsS3Factory* s3_factory_;
  std::optional<AwsBlobStorageClientProvider> provider_;
  std::shared_ptr<MockS3Client> s3_client_;

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context_;

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context_;

  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  absl::Mutex finish_called_mu_;
  bool finish_called_ ABSL_GUARDED_BY(finish_called_mu_) = false;

  SDKOptions options_;
};

MATCHER_P2(HasBucketAndKey, bucket, key, "") {
  return ExplainMatchResult(Eq(bucket), arg.GetBucket(), result_listener) &&
         ExplainMatchResult(Eq(key), arg.GetKey(), result_listener);
}

TEST_F(AwsBlobStorageClientProviderTest,
       RunWithCreateClientConfigurationFailed) {
  instance_client_.get_instance_resource_name_mock = absl::UnknownError("");

  EXPECT_FALSE(provider_->Init().ok());
}

TEST_F(AwsBlobStorageClientProviderTest, GetBlobFailure) {
  std::string_view bucket_name = "bucket_name";
  std::string_view blob_name = "blob_name";
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      bucket_name);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(blob_name);
  get_blob_context_.callback =
      [this](AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
        EXPECT_THAT(
            get_blob_context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_CALL(*s3_client_,
              GetObjectAsync(HasBucketAndKey(bucket_name, blob_name), _, _))
      .WillOnce([](auto, auto callback, auto) {
        GetObjectRequest get_object_request;
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        GetObjectOutcome get_object_outcome(s3_error);
        callback(nullptr /*s3_client*/, get_object_request,
                 std::move(get_object_outcome), nullptr /*async_context*/);
      });

  EXPECT_TRUE(provider_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(AwsBlobStorageClientProviderTest, GetBlobSuccess) {
  std::string_view bucket_name = "bucket_name";
  std::string_view blob_name = "blob_name";
  std::string blob_data("Hello world!");
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      bucket_name);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(blob_name);
  get_blob_context_.callback =
      [this, &bucket_name, &blob_name, &blob_data](
          AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
        ASSERT_SUCCESS(get_blob_context.result);
        EXPECT_THAT(get_blob_context.response->blob().metadata().bucket_name(),
                    StrEq(bucket_name));
        EXPECT_THAT(get_blob_context.response->blob().metadata().blob_name(),
                    StrEq(blob_name));
        EXPECT_THAT(get_blob_context.response->blob().data(), StrEq(blob_data));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_CALL(*s3_client_,
              GetObjectAsync(HasBucketAndKey(bucket_name, blob_name), _, _))
      .WillOnce([&blob_data](auto, auto callback, auto) {
        GetObjectRequest get_object_request;
        GetObjectResult get_object_result;
        StringStream* input_data = new StringStream("");
        *input_data << blob_data;

        get_object_result.ReplaceBody(input_data);
        get_object_result.SetContentLength(12);
        GetObjectOutcome get_object_outcome(std::move(get_object_result));
        callback(nullptr /*s3_client*/, get_object_request,
                 std::move(get_object_outcome), nullptr /*async_context*/);
      });

  EXPECT_TRUE(provider_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

MATCHER_P3(HasBucketKeyAndRange, bucket, key, range, "") {
  return ExplainMatchResult(HasBucketAndKey(bucket, key), arg,
                            result_listener) &&
         ExplainMatchResult(Eq(range), arg.GetRange(), result_listener);
}

TEST_F(AwsBlobStorageClientProviderTest, GetBlobWithByteRange) {
  std::string_view bucket_name = "bucket_name";
  std::string_view blob_name = "blob_name";
  std::string blob_data("Hello world!");
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      bucket_name);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(blob_name);
  get_blob_context_.request->mutable_byte_range()->set_begin_byte_index(5);
  get_blob_context_.request->mutable_byte_range()->set_end_byte_index(100);
  get_blob_context_.callback =
      [this, &bucket_name, &blob_name, &blob_data](
          AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) {
        ASSERT_SUCCESS(get_blob_context.result);
        EXPECT_THAT(get_blob_context.response->blob().metadata().bucket_name(),
                    StrEq(bucket_name));
        EXPECT_THAT(get_blob_context.response->blob().metadata().blob_name(),
                    StrEq(blob_name));
        EXPECT_THAT(get_blob_context.response->blob().data(), StrEq(blob_data));

        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_CALL(
      *s3_client_,
      GetObjectAsync(
          HasBucketKeyAndRange(bucket_name, blob_name, "bytes=5-100"), _, _))
      .WillOnce([&blob_data](auto, auto callback, auto) {
        GetObjectRequest get_object_request;
        GetObjectResult get_object_result;
        StringStream* input_data = new StringStream("");
        *input_data << blob_data;

        get_object_result.ReplaceBody(input_data);
        get_object_result.SetContentLength(12);
        GetObjectOutcome get_object_outcome(std::move(get_object_result));
        callback(nullptr /*s3_client*/, get_object_request,
                 std::move(get_object_outcome), nullptr /*async_context*/);
      });

  EXPECT_TRUE(provider_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

MATCHER_P3(HasBucketKeyAndBody, bucket_name, key, body, "") {
  std::string body_string(body.length(), 0);
  arg.GetBody()->read(body_string.data(), body_string.length());
  return ExplainMatchResult(HasBucketAndKey(bucket_name, key), arg,
                            result_listener) &&
         ExplainMatchResult(Eq(body), body_string, result_listener);
}

TEST_F(AwsBlobStorageClientProviderTest, PutBlobFailure) {
  std::string_view bucket_name = "bucket_name";
  std::string_view blob_name = "blob_name";

  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(bucket_name);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      blob_name);

  std::string body_str("1234567890");
  put_blob_context_.request->mutable_blob()->set_data(body_str);
  put_blob_context_.callback =
      [this](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        EXPECT_THAT(
            put_blob_context.result,
            ResultIs(FailureExecutionResult(SC_AWS_INTERNAL_SERVICE_ERROR)));
        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_CALL(*s3_client_,
              PutObjectAsync(
                  HasBucketKeyAndBody(bucket_name, blob_name, body_str), _, _))
      .WillOnce([](auto, auto& callback, auto) {
        PutObjectRequest put_object_request;
        AWSError<S3Errors> s3_error(S3Errors::ACCESS_DENIED, false);
        PutObjectOutcome put_object_outcome(s3_error);
        callback(nullptr /*s3_client*/, put_object_request,
                 std::move(put_object_outcome), nullptr /*async_context*/);
      });

  EXPECT_TRUE(provider_->PutBlob(put_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(AwsBlobStorageClientProviderTest, PutBlobSuccess) {
  std::string_view bucket_name = "bucket_name";
  std::string_view blob_name = "blob_name";

  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(bucket_name);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      blob_name);

  std::string body_str("1234567890");
  put_blob_context_.request->mutable_blob()->set_data(body_str);
  put_blob_context_.callback =
      [this](AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) {
        EXPECT_SUCCESS(put_blob_context.result);
        absl::MutexLock lock(&finish_called_mu_);
        finish_called_ = true;
      };

  EXPECT_CALL(*s3_client_,
              PutObjectAsync(
                  HasBucketKeyAndBody(bucket_name, blob_name, body_str), _, _))
      .WillOnce([](auto, auto& callback, auto) {
        PutObjectRequest put_object_request;
        PutObjectResult result;
        PutObjectOutcome put_object_outcome(std::move(result));
        callback(nullptr /*s3_client*/, put_object_request,
                 std::move(put_object_outcome), nullptr /*async_context*/);
      });

  EXPECT_TRUE(provider_->PutBlob(put_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

}  // namespace google::scp::cpio::client_providers
