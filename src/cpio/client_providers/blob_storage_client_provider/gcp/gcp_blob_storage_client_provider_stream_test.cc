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

#include <atomic>
#include <string>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/base/thread_annotations.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/synchronization/notification.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/hashing.h"
#include "src/cpio/client_providers/blob_storage_client_provider/common/error_codes.h"
#include "src/cpio/client_providers/blob_storage_client_provider/gcp/gcp_blob_storage_client_provider.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::cloud::Status;
using google::cloud::StatusOr;
using CloudStatusCode = google::cloud::StatusCode;
using google::cloud::storage::Crc32cChecksumValue;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::DisableMD5Hash;
using google::cloud::storage::LimitedErrorCountRetryPolicy;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::Prefix;
using google::cloud::storage::ReadRange;
using google::cloud::storage::internal::ConstBuffer;
using google::cloud::storage::internal::ConstBufferSequence;
using google::cloud::storage::internal::CreateHashFunction;
using google::cloud::storage::internal::CreateResumableUploadResponse;
using google::cloud::storage::internal::HttpResponse;
using google::cloud::storage::internal::ObjectReadSource;
using google::cloud::storage::internal::QueryResumableUploadResponse;
using google::cloud::storage::internal::ReadSourceResult;
using google::cloud::storage::internal::ResumableUploadRequest;
using google::cloud::storage::internal::UploadChunkRequest;
using google::cloud::storage::testing::ClientFromMock;
using google::cloud::storage::testing::MockClient;
using google::cloud::storage::testing::MockObjectReadSource;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncExecutor;
using google::scp::core::BytesBuffer;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::RetryExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::common::ConcurrentQueue;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::errors::SC_GCP_UNKNOWN;
using google::scp::core::errors::SC_STREAMING_CONTEXT_DONE;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::core::utils::CalculateMd5Hash;
using google::scp::cpio::client_providers::GcpBlobStorageClientProvider;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using testing::ByMove;
using testing::ElementsAre;
using testing::ElementsAreArray;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::NiceMock;
using testing::Pointwise;
using testing::Return;

namespace google::scp::cpio::client_providers::test {
namespace {
constexpr std::string_view kInstanceResourceName =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr char kBucketName[] = "bucket";
constexpr char kBlobName[] = "blob";

constexpr int64_t kStreamKeepAliveMicrosCount = 100;
// GCS requires chunks to be a multiple of 256 KiB.
// Hardcoded the value of GOOGLE_CLOUD_CPP_STORAGE_DEFAULT_UPLOAD_BUFFER_SIZE
// from google/cloud/storage/client_options.cc which is not publicly visible.
constexpr size_t kUploadSize = 8 * 1024 * 1024;

class MockGcpCloudStorageFactory : public GcpCloudStorageFactory {
 public:
  MOCK_METHOD(
      core::ExecutionResultOr<std::unique_ptr<google::cloud::storage::Client>>,
      CreateClient, (BlobStorageClientOptions), (noexcept, override));
};

class GcpBlobStorageClientProviderStreamTest : public testing::Test {
 protected:
  GcpBlobStorageClientProviderStreamTest()
      : mock_client_(std::make_shared<NiceMock<MockClient>>()) {
    auto storage_factory =
        std::make_unique<NiceMock<MockGcpCloudStorageFactory>>();
    storage_factory_ = storage_factory.get();
    BlobStorageClientOptions options;
    gcp_cloud_storage_client_.emplace(std::move(options), &instance_client_,
                                      &cpu_async_executor_, &io_async_executor_,
                                      std::move(storage_factory));
    ON_CALL(*storage_factory_, CreateClient)
        .WillByDefault(
            Return(ByMove(std::make_unique<google::cloud::storage::Client>(
                ClientFromMock(mock_client_)))));
    instance_client_.instance_resource_name = kInstanceResourceName;
    get_blob_stream_context_.request = std::make_unique<GetBlobStreamRequest>();
    get_blob_stream_context_.process_callback = [this](auto, bool) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    put_blob_stream_context_.request = std::make_unique<PutBlobStreamRequest>();
    put_blob_stream_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    EXPECT_TRUE(gcp_cloud_storage_client_->Init().ok());
  }

  MockInstanceClientProvider instance_client_;
  MockAsyncExecutor cpu_async_executor_;
  MockAsyncExecutor io_async_executor_;
  MockGcpCloudStorageFactory* storage_factory_;
  std::shared_ptr<MockClient> mock_client_;
  std::optional<GcpBlobStorageClientProvider> gcp_cloud_storage_client_;

  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
      get_blob_stream_context_;

  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
      put_blob_stream_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  absl::Mutex finish_called_mu_;
  bool finish_called_ ABSL_GUARDED_BY(finish_called_mu_) = false;
};

///////////// GetBlobStream ///////////////////////////////////////////////////

// Compares 2 BlobMetadata's bucket_name and blob_name.
MATCHER_P(BlobMetadataEquals, expected_metadata, "") {
  return ExplainMatchResult(arg.bucket_name(), expected_metadata.bucket_name(),
                            result_listener) &&
         ExplainMatchResult(arg.blob_name(), expected_metadata.blob_name(),
                            result_listener);
}

// Compares 2 Blobs, their metadata and data.
MATCHER_P(BlobEquals, expected_blob, "") {
  return ExplainMatchResult(BlobMetadataEquals(expected_blob.metadata()),
                            arg.metadata(), result_listener) &&
         ExplainMatchResult(expected_blob.data(), arg.data(), result_listener);
}

MATCHER_P(GetBlobStreamResponseEquals, expected, "") {
  return ExplainMatchResult(BlobEquals(expected.blob_portion()),
                            arg.blob_portion(), result_listener) &&
         ExplainMatchResult(expected.byte_range().begin_byte_index(),
                            arg.byte_range().begin_byte_index(),
                            result_listener) &&
         ExplainMatchResult(expected.byte_range().end_byte_index(),
                            arg.byte_range().end_byte_index(), result_listener);
}

MATCHER(GetBlobStreamResponseEquals, "") {
  const auto& [actual, expected] = arg;
  return ExplainMatchResult(GetBlobStreamResponseEquals(expected), actual,
                            result_listener);
}

// Matches arg.bucket_name and arg.object_name with bucket_name and
// blob_name respectively. Also ensures that arg has DisableMD5Hash = false
// and DisableCrc32cChecksum = true.
MATCHER_P2(ReadObjectRequestEqual, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(blob_name), arg.object_name(), result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<DisableMD5Hash>() ||
      arg.template GetOption<DisableMD5Hash>().value()) {
    *result_listener << "Expected ReadObjectRequest to have DisableMD5Hash == "
                        "false and it does not.";
    equal = false;
  }
  if (!arg.template HasOption<DisableCrc32cChecksum>() ||
      !arg.template GetOption<DisableCrc32cChecksum>().value()) {
    *result_listener << "Expected ReadObjectRequest to have "
                        "DisableCrc32cChecksum == true and it does not.";
    equal = false;
  }
  return equal;
}

// Builds an ObjectReadSource that contains the bytes (copied) from bytes_str.
StatusOr<std::unique_ptr<ObjectReadSource>> BuildReadResponseFromString(
    std::string_view bytes_str) {
  // We want the following methods to be called in order, so make an InSequence.
  InSequence seq;
  auto mock_source = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(true));
  // Copy up to n bytes from input into buf.
  EXPECT_CALL(*mock_source, Read)
      .WillOnce([bytes_str = bytes_str](void* buf, std::size_t n) {
        BytesBuffer buffer(bytes_str.length());
        buffer.bytes->assign(bytes_str.begin(), bytes_str.end());
        buffer.length = bytes_str.length();
        auto length = std::min(buffer.length, n);
        std::memcpy(buf, buffer.bytes->data(), length);
        ReadSourceResult result{length, HttpResponse{200, {}, {}}};

        result.hashes.md5 = *CalculateMd5Hash(buffer);
        Base64Encode(result.hashes.md5, result.hashes.md5);

        result.size = length;
        return result;
      });
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));
  return std::unique_ptr<ObjectReadSource>(std::move(mock_source));
}

TEST_F(GcpBlobStorageClientProviderStreamTest, GetBlobStream) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);

  // 15 chars.
  std::string bytes_str = "response_string";
  GetBlobStreamResponse expected_response;
  expected_response.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  *expected_response.mutable_blob_portion()->mutable_data() = bytes_str;
  expected_response.mutable_byte_range()->set_begin_byte_index(0);
  expected_response.mutable_byte_range()->set_end_byte_index(14);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  std::vector<GetBlobStreamResponse> actual_responses;
  get_blob_stream_context_.process_callback = [this, &actual_responses](
                                                  auto& context, bool) {
    auto resp = context.TryGetNextResponse();
    if (resp != nullptr) {
      actual_responses.push_back(std::move(*resp));
    } else {
      if (!context.IsMarkedDone()) {
        ADD_FAILURE();
      }
      EXPECT_SUCCESS(context.result);
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    }
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->GetBlobStream(get_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              ElementsAre(GetBlobStreamResponseEquals(expected_response)));
}

TEST_F(GcpBlobStorageClientProviderStreamTest, GetBlobStreamMultipleResponses) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->set_max_bytes_per_response(2);

  // 15 chars.
  std::string bytes_str = "response_string";
  // Expect to get responses with data: ["re", "sp", ... "g"]
  std::vector<GetBlobStreamResponse> expected_responses;
  for (int i = 0; i < bytes_str.length(); i += 2) {
    GetBlobStreamResponse resp;
    resp.mutable_blob_portion()->mutable_metadata()->CopyFrom(
        get_blob_stream_context_.request->blob_metadata());
    // The last 1 character is by itself
    if (i + 1 == bytes_str.length()) {
      *resp.mutable_blob_portion()->mutable_data() = bytes_str.substr(i, 1);
      resp.mutable_byte_range()->set_begin_byte_index(i);
      resp.mutable_byte_range()->set_end_byte_index(i);
    } else {
      *resp.mutable_blob_portion()->mutable_data() = bytes_str.substr(i, 2);
      resp.mutable_byte_range()->set_begin_byte_index(i);
      resp.mutable_byte_range()->set_end_byte_index(i + 1);
    }
    expected_responses.push_back(resp);
  }

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  std::vector<GetBlobStreamResponse> actual_responses;
  get_blob_stream_context_.process_callback = [this, &actual_responses](
                                                  auto& context, bool) {
    auto resp = context.TryGetNextResponse();
    if (resp != nullptr) {
      actual_responses.push_back(std::move(*resp));
    } else {
      if (!context.IsMarkedDone()) {
        ADD_FAILURE();
      }
      EXPECT_SUCCESS(context.result);
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    }
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->GetBlobStream(get_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              Pointwise(GetBlobStreamResponseEquals(), expected_responses));
}

TEST_F(GcpBlobStorageClientProviderStreamTest, GetBlobStreamByteRange) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.request->set_max_bytes_per_response(3);
  get_blob_stream_context_.request->mutable_byte_range()->set_begin_byte_index(
      3);
  get_blob_stream_context_.request->mutable_byte_range()->set_end_byte_index(6);

  // We slice "response_string" to indices 3-6. We pad "a" at the end so
  // "content_length" is still 15 to simulate a ranged read.
  std::string bytes_str = "ponsaaaaaaaaaaa";
  // Expect to get responses with data: ["pon", "s"]
  std::vector<GetBlobStreamResponse> expected_responses;
  GetBlobStreamResponse resp1, resp2;
  resp1.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  resp2.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      get_blob_stream_context_.request->blob_metadata());
  *resp1.mutable_blob_portion()->mutable_data() = "pon";
  resp1.mutable_byte_range()->set_begin_byte_index(3);
  resp1.mutable_byte_range()->set_end_byte_index(5);
  *resp2.mutable_blob_portion()->mutable_data() = "s";
  resp2.mutable_byte_range()->set_begin_byte_index(6);
  resp2.mutable_byte_range()->set_end_byte_index(6);
  expected_responses.push_back(resp1);
  expected_responses.push_back(resp2);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  std::vector<GetBlobStreamResponse> actual_responses;
  get_blob_stream_context_.process_callback = [this, &actual_responses](
                                                  auto& context, bool) {
    auto resp = context.TryGetNextResponse();
    if (resp != nullptr) {
      actual_responses.push_back(std::move(*resp));
    } else {
      if (!context.IsMarkedDone()) {
        ADD_FAILURE();
      }
      EXPECT_SUCCESS(context.result);
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    }
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->GetBlobStream(get_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
  EXPECT_THAT(actual_responses,
              Pointwise(GetBlobStreamResponseEquals(), expected_responses));
}

TEST_F(GcpBlobStorageClientProviderStreamTest, GetBlobStreamFailsIfQueueDone) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.MarkDone();

  // 15 chars.
  std::string bytes_str = "response_string";

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_STREAMING_CONTEXT_DONE)));
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->GetBlobStream(get_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpBlobStorageClientProviderStreamTest,
       GetBlobStreamFailsIfRequestCancelled) {
  get_blob_stream_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_stream_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName);
  get_blob_stream_context_.TryCancel();

  // 15 chars.
  std::string bytes_str = "response_string";

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  get_blob_stream_context_.process_callback = [this](auto& context, bool) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED)));
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->GetBlobStream(get_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(get_blob_stream_context_.IsMarkedDone());
}

///////////// PutBlobStream ///////////////////////////////////////////////////

MATCHER_P(CreateResumableUploadEquals, expected, "") {
  return ExplainMatchResult(expected.bucket_name(), arg.bucket_name(),
                            result_listener) &&
         ExplainMatchResult(expected.object_name(), arg.object_name(),
                            result_listener);
}

MATCHER_P(UploadChunkEquals, expected, "") {
  return ExplainMatchResult(expected.upload_session_url(),
                            arg.upload_session_url(), result_listener) &&
         ExplainMatchResult(expected.offset(), arg.offset(), result_listener) &&
         ExplainMatchResult(ElementsAreArray(expected.payload()), arg.payload(),
                            result_listener);
}

// Note, string s is not copied or moved - it is referred to wherever this
// buffer exists. Therefore, s must continue to be valid while this buffer
// exists.
ConstBufferSequence MakeBuffer(std::string_view s) {
  return ConstBufferSequence{ConstBuffer(s.data(), s.length())};
}

ConstBufferSequence EmptyBuffer() { return ConstBufferSequence(); }

MATCHER_P(HasSessionUrl, url, "") {
  return ExplainMatchResult(url, arg.upload_session_url(), result_listener);
}

/**
 * @brief Expects calls to mock_client resembling a resumable upload process.
 * Generally this is the process:
 * 1. CreateResumableUpload
 * 2. (optional) RestoreResumableUpload
 * 3. UploadChunk
 * 4. Loop back to 2
 *
 * @param mock_client Client to set expectations on.
 * @param bucket The name of the bucket these operations occur on.
 * @param blob The name of the blob these operations occur on.
 * @param initial_part The initial data contained in the first request.
 * @param other_parts List of other chunks of data to expect.
 * @param expect_queries Whether or not to expect RestoreResumableUpload's
 * before *each* UploadChunk call.
 */
void ExpectResumableUpload(MockClient& mock_client, std::string_view bucket,
                           std::string_view blob, std::string_view initial_part,
                           const std::vector<std::string>& other_parts,
                           bool expect_queries = false) {
  static int upload_count = 0;
  auto session_id = absl::StrCat("session_", upload_count++);
  InSequence seq;

  // First, create a session and upload the initial part.
  uint64_t next_offset = initial_part.length();
  EXPECT_CALL(
      mock_client,
      CreateResumableUpload(CreateResumableUploadEquals(
          ResumableUploadRequest(std::string(bucket), std::string(blob)))))
      .WillOnce(Return(CreateResumableUploadResponse{session_id}));

  EXPECT_CALL(
      mock_client,
      UploadChunk(UploadChunkEquals(UploadChunkRequest(
          session_id, 0, MakeBuffer(initial_part),
          CreateHashFunction(Crc32cChecksumValue(), DisableCrc32cChecksum(true),
                             MD5HashValue(), DisableMD5Hash(true))))))
      .WillOnce(
          Return(QueryResumableUploadResponse{next_offset, std::nullopt}));

  // For each of the other parts, we expect to get another UploadChunk call.
  for (auto it = other_parts.begin(); it != other_parts.end(); it++) {
    if (expect_queries) {
      EXPECT_CALL(mock_client, QueryResumableUpload(HasSessionUrl(session_id)))
          .WillRepeatedly(
              Return(QueryResumableUploadResponse{next_offset, std::nullopt}));
    }
    EXPECT_CALL(mock_client,
                UploadChunk(UploadChunkEquals(UploadChunkRequest(
                    session_id, next_offset, MakeBuffer(*it),
                    CreateHashFunction(Crc32cChecksumValue(),
                                       DisableCrc32cChecksum(true),
                                       MD5HashValue(), DisableMD5Hash(true))))))
        .WillOnce(Return(QueryResumableUploadResponse{
            next_offset + it->length(), std::nullopt}));
    next_offset += it->length();
  }
  // Finalization call - no body but should return ObjectMetadata.
  EXPECT_CALL(
      mock_client,
      UploadChunk(UploadChunkEquals(UploadChunkRequest(
          session_id, next_offset, EmptyBuffer(),
          CreateHashFunction(Crc32cChecksumValue(), DisableCrc32cChecksum(true),
                             MD5HashValue(), DisableMD5Hash(true))))))
      .WillOnce(
          Return(QueryResumableUploadResponse{next_offset, ObjectMetadata{}}));
}

TEST_F(GcpBlobStorageClientProviderStreamTest, PutBlobStream) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  std::string bytes_str = "initial";
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.MarkDone();

  ExpectResumableUpload(*mock_client_, kBucketName, kBlobName, bytes_str, {});

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderStreamTest, PutBlobStreamMultiplePortions) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  // The API will optimize uploads to kUploadSize bytes, we test our
  // implementation by making each part that size.
  std::string initial_str(kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(
      initial_str);

  std::vector<std::string> strings{std::string(kUploadSize, 'b'),
                                   std::string(kUploadSize, 'c')};
  auto request2 = *put_blob_stream_context_.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  auto request3 = *put_blob_stream_context_.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  put_blob_stream_context_.TryPushRequest(std::move(request2));
  put_blob_stream_context_.TryPushRequest(std::move(request3));
  put_blob_stream_context_.MarkDone();

  ExpectResumableUpload(*mock_client_, kBucketName, kBlobName, initial_str,
                        strings);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderStreamTest,
       PutBlobStreamFailsIfInitialWriteFails) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  std::string bytes_str(kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.MarkDone();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(Status(CloudStatusCode::kResourceExhausted, "fail")));

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpBlobStorageClientProviderStreamTest,
       PutBlobStreamFailsIfSubsequentWriteFails) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  std::string bytes_str(kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Place another request on the context.
  put_blob_stream_context_.TryPushRequest(*put_blob_stream_context_.request);
  put_blob_stream_context_.MarkDone();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}))
      .WillOnce(Return(Status(CloudStatusCode::kResourceExhausted, "fail")));

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_UNKNOWN)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpBlobStorageClientProviderStreamTest,
       PutBlobStreamFailsIfFinalizingFails) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  std::string bytes_str(kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Place another request on the context.
  put_blob_stream_context_.TryPushRequest(*put_blob_stream_context_.request);
  put_blob_stream_context_.MarkDone();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}))
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length() * 2, std::nullopt}))
      .WillOnce(Return(Status(CloudStatusCode::kInternal, "fail")));

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result, ResultIs(RetryExecutionResult(
                                    SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  {
    absl::MutexLock lock(&finish_called_mu_);
    finish_called_mu_.Await(absl::Condition(&finish_called_));
  }
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpBlobStorageClientProviderStreamTest,
       PutBlobStreamFailsIfStreamExpires) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);
  *put_blob_stream_context_.request->mutable_stream_keepalive_duration() =
      TimeUtil::MicrosecondsToDuration(kStreamKeepAliveMicrosCount);

  std::string bytes_str(kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // Don't mark the context as done and don't enqueue any messages.

  InSequence seq;
  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}));
  EXPECT_CALL(*mock_client_, DeleteResumableUpload);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  std::this_thread::sleep_for(
      std::chrono::microseconds(kStreamKeepAliveMicrosCount));
  absl::MutexLock lock(&finish_called_mu_);
  EXPECT_TRUE(finish_called_);
  EXPECT_TRUE(put_blob_stream_context_.IsMarkedDone());
}

TEST_F(GcpBlobStorageClientProviderStreamTest, PutBlobStreamFailsIfCancelled) {
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context_.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  std::string bytes_str(kUploadSize, 'a');
  put_blob_stream_context_.request->mutable_blob_portion()->set_data(bytes_str);
  // No additional request objects.
  put_blob_stream_context_.TryCancel();

  EXPECT_CALL(*mock_client_, CreateResumableUpload)
      .WillOnce(Return(CreateResumableUploadResponse{"something"}));
  EXPECT_CALL(*mock_client_, UploadChunk)
      .WillOnce(Return(
          QueryResumableUploadResponse{bytes_str.length(), std::nullopt}));
  EXPECT_CALL(*mock_client_, DeleteResumableUpload);

  put_blob_stream_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->PutBlobStream(put_blob_stream_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST(GcpBlobStorageClientProviderStreamTestII,
     PutBlobStreamMultiplePortionsWithNoOpCycles) {
  // In order to test the "no message" path, we must have real async executors.
  AsyncExecutor cpu_async_executor(1, 5);
  AsyncExecutor io_async_executor(1, 5);
  MockInstanceClientProvider instance_client;
  instance_client.instance_resource_name = kInstanceResourceName;
  std::shared_ptr<MockClient> mock_client =
      std::make_shared<NiceMock<MockClient>>();
  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
      put_blob_stream_context;
  auto storage_factory =
      std::make_unique<NiceMock<MockGcpCloudStorageFactory>>();
  auto storage_factory_ptr = storage_factory.get();
  GcpBlobStorageClientProvider gcp_cloud_storage_client(
      BlobStorageClientOptions(), &instance_client, &cpu_async_executor,
      &io_async_executor, std::move(storage_factory));
  ON_CALL(*storage_factory_ptr, CreateClient)
      .WillByDefault(
          Return(ByMove(std::make_unique<google::cloud::storage::Client>(
              ClientFromMock(mock_client)))));

  EXPECT_TRUE(gcp_cloud_storage_client.Init().ok());

  put_blob_stream_context.request = std::make_unique<PutBlobStreamRequest>();
  put_blob_stream_context.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_stream_context.request->mutable_blob_portion()
      ->mutable_metadata()
      ->set_blob_name(kBlobName);

  // The API will optimize uploads to kUploadSize bytes, we test our
  // implementation by making each part that size.
  std::string initial_str(kUploadSize, 'a');
  put_blob_stream_context.request->mutable_blob_portion()->set_data(
      initial_str);

  std::vector<std::string> strings{std::string(kUploadSize, 'b'),
                                   std::string(kUploadSize, 'c')};

  ExpectResumableUpload(*mock_client, kBucketName, kBlobName, initial_str,
                        strings, true /*expect_queries*/);

  absl::Notification finished;
  put_blob_stream_context.callback = [&finished](auto& context) {
    EXPECT_SUCCESS(context.result);
    finished.Notify();
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client.PutBlobStream(put_blob_stream_context).ok());
  // After this point, the client is waiting for the context to be done, which
  // it is not.

  // Wait until the stream has been suspended.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto request2 = *put_blob_stream_context.request;
  request2.mutable_blob_portion()->set_data(strings[0]);
  put_blob_stream_context.TryPushRequest(std::move(request2));
  std::cout << "5555" << std::endl;

  // Wait until the stream has been suspended.
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  auto request3 = *put_blob_stream_context.request;
  request3.mutable_blob_portion()->set_data(strings[1]);
  put_blob_stream_context.TryPushRequest(std::move(request3));
  put_blob_stream_context.MarkDone();

  finished.WaitForNotification();
}

}  // namespace
}  // namespace google::scp::cpio::client_providers::test
