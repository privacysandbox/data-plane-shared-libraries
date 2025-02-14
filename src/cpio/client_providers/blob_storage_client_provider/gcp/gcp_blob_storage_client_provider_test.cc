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

#include "src/cpio/client_providers/blob_storage_client_provider/gcp/gcp_blob_storage_client_provider.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "google/cloud/storage/testing/mock_client.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/interface/type_def.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/hashing.h"
#include "src/cpio/client_providers/blob_storage_client_provider/common/error_codes.h"
#include "src/cpio/client_providers/instance_client_provider/mock/mock_instance_client_provider.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::cloud::Status;
using google::cloud::StatusOr;
using CloudStatusCode = google::cloud::StatusCode;
using google::cloud::storage::Client;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::DisableMD5Hash;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::Prefix;
using google::cloud::storage::ReadRange;
using google::cloud::storage::StartOffset;
using google::cloud::storage::internal::EmptyResponse;
using google::cloud::storage::internal::HttpResponse;
using google::cloud::storage::internal::InsertObjectMediaRequest;
using google::cloud::storage::internal::ListObjectsResponse;
using google::cloud::storage::internal::ObjectReadSource;
using google::cloud::storage::internal::ReadSourceResult;
using google::cloud::storage::testing::ClientFromMock;
using google::cloud::storage::testing::MockClient;
using google::cloud::storage::testing::MockObjectReadSource;
using google::cmrt::sdk::blob_storage_service::v1::Blob;
using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::scp::core::AsyncContext;
using google::scp::core::Byte;
using google::scp::core::FailureExecutionResult;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR;
using google::scp::core::errors::SC_GCP_DATA_LOSS;
using google::scp::core::errors::SC_GCP_NOT_FOUND;
using google::scp::core::test::IsSuccessful;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Encode;
using google::scp::core::utils::CalculateMd5Hash;
using google::scp::cpio::client_providers::GcpBlobStorageClientProvider;
using google::scp::cpio::client_providers::GcpCloudStorageFactory;
using google::scp::cpio::client_providers::mock::MockInstanceClientProvider;
using testing::ByMove;
using testing::ElementsAre;
using testing::Eq;
using testing::ExplainMatchResult;
using testing::InSequence;
using testing::IsNull;
using testing::NiceMock;
using testing::NotNull;
using testing::Pointwise;
using testing::Return;

namespace {
constexpr std::string_view kProjectIdValueMock = "123456789";
constexpr std::string_view kInstanceResourceName =
    R"(//compute.googleapis.com/projects/123456789/zones/us-central1-c/instances/987654321)";
constexpr std::string_view kBucketName = "bucket";
constexpr std::string_view kBlobName1 = "blob_1";
constexpr std::string_view kBlobName2 = "blob_2";
constexpr std::string_view kBlobNameEmpty = "blob_empty";

constexpr uint64_t kDefaultMaxPageSize = 1000;
}  // namespace

namespace google::scp::cpio::client_providers::test {

class MockGcpCloudStorageFactory : public GcpCloudStorageFactory {
 public:
  MOCK_METHOD(core::ExecutionResultOr<std::unique_ptr<Client>>, CreateClient,
              (BlobStorageClientOptions), (noexcept, override));
};

// Params:
// <begin_index, end_index, actual std::string to return, expected std::string
// to observe>
class GcpBlobStorageClientProviderTest
    : public testing::TestWithParam<
          std::tuple<uint64_t, uint64_t, std::string, std::string>> {
 protected:
  GcpBlobStorageClientProviderTest()
      : mock_client_(std::make_shared<NiceMock<MockClient>>()) {
    auto storage_factory =
        std::make_unique<NiceMock<MockGcpCloudStorageFactory>>();
    storage_factory_ = storage_factory.get();
    BlobStorageClientOptions options;
    options.project_id = kProjectIdValueMock;
    gcp_cloud_storage_client_.emplace(std::move(options), &instance_client_,
                                      &cpu_async_executor_, &io_async_executor_,
                                      std::move(storage_factory));
    ON_CALL(*storage_factory_, CreateClient)
        .WillByDefault(Return(
            ByMove(std::make_unique<Client>(ClientFromMock(mock_client_)))));
    instance_client_.instance_resource_name = kInstanceResourceName;
    get_blob_context_.request = std::make_shared<GetBlobRequest>();
    get_blob_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    list_blobs_context_.request = std::make_shared<ListBlobsMetadataRequest>();
    list_blobs_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    put_blob_context_.request = std::make_shared<PutBlobRequest>();
    put_blob_context_.callback = [this](auto) {
      absl::MutexLock lock(&finish_called_mu_);
      finish_called_ = true;
    };

    delete_blob_context_.request = std::make_shared<DeleteBlobRequest>();
    delete_blob_context_.callback = [this](auto) {
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

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context_;

  AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
      list_blobs_context_;

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context_;

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context_;
  // We check that this gets flipped after every call to ensure the context's
  // Finish() is called.
  absl::Mutex finish_called_mu_;
  bool finish_called_ ABSL_GUARDED_BY(finish_called_mu_) = false;
};

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

///////////// GetBlob /////////////////////////////////////////////////////////

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
        auto buffer = std::make_shared<std::string>(bytes_str);
        auto length = std::min(buffer->size(), n);
        std::memcpy(buf, buffer->c_str(), length);
        ReadSourceResult result{length, HttpResponse{200, {}, {}}};

        result.hashes.md5 = *CalculateMd5Hash(buffer);
        Base64Encode(result.hashes.md5, result.hashes.md5);

        result.size = length;
        return result;
      });
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));
  return std::unique_ptr<ObjectReadSource>(std::move(mock_source));
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

TEST_F(GcpBlobStorageClientProviderTest, GetBlob) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);

  std::string bytes_str = "response_string";

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName1)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(bytes_str))));

  get_blob_context_.callback = [this, &bytes_str](auto& context) {
    ASSERT_SUCCESS(context.result);

    Blob expected_blob;
    expected_blob.mutable_metadata()->set_bucket_name(kBucketName);
    expected_blob.mutable_metadata()->set_blob_name(kBlobName1);
    expected_blob.set_data(bytes_str);

    ASSERT_THAT(context.response, testing::NotNull());
    EXPECT_THAT(context.response->blob(), BlobEquals(expected_blob));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

MATCHER_P4(ReadObjectRequestEqualsWithRange, bucket_name, blob_name,
           begin_index, end_index, "") {
  bool equal = true;
  if (!ExplainMatchResult(ReadObjectRequestEqual(bucket_name, blob_name), arg,
                          result_listener)) {
    equal = false;
  }

  if (!arg.template HasOption<ReadRange>() ||
      arg.template GetOption<ReadRange>().value().begin != begin_index ||
      arg.template GetOption<ReadRange>().value().end != end_index) {
    *result_listener << "Expected ReadObjectRequest to have ReadRange ("
                     << begin_index << ", " << end_index << ") but does not.";
    equal = false;
  }
  return equal;
}

TEST_P(GcpBlobStorageClientProviderTest, GetBlobWithByteRange) {
  const auto& [begin_index, end_index, actual_str, expected_str] = GetParam();
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);
  get_blob_context_.request->mutable_byte_range()->set_begin_byte_index(
      begin_index);
  get_blob_context_.request->mutable_byte_range()->set_end_byte_index(
      end_index);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqualsWithRange(
                  kBucketName, kBlobName1, begin_index, end_index + 1)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(actual_str))));

  get_blob_context_.callback = [this,
                                expected_str = expected_str](auto& context) {
    ASSERT_SUCCESS(context.result);

    Blob expected_blob;
    expected_blob.mutable_metadata()->set_bucket_name(kBucketName);
    expected_blob.mutable_metadata()->set_blob_name(kBlobName1);
    expected_blob.set_data(expected_str);

    ASSERT_THAT(context.response, testing::NotNull());
    EXPECT_THAT(context.response->blob(), BlobEquals(expected_blob));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

// Imagine the existing blob has data "0123456789". We exercise different cases
// for ranged reads on it.
// The tuples are <begin_index, end_index, string to return, expected string>
// We pad 'a' to the string to return so that the content length is always 10.
INSTANTIATE_TEST_SUITE_P(
    ByteRangeTest, GcpBlobStorageClientProviderTest,
    testing::Values(
        // Range is full length of object.
        std::make_tuple(0, 9, "0123456789", "0123456789"),
        // Range starts at offset.
        std::make_tuple(2, 9, "23456789aa", "23456789"),
        // Range ends at offset.
        std::make_tuple(0, 7, "01234567aa", "01234567"),
        // Range is a shifted window - "aa" should be ignored.
        std::make_tuple(2, 11, "23456789aa", "23456789"),
        // Range is longer than object length - "aa" should be ignored
        std::make_tuple(2, 15, "23456789aa", "23456789")));

StatusOr<std::unique_ptr<ObjectReadSource>> BuildBadHashReadResponse() {
  // We want the following methods to be called in order, so make an
  InSequence seq;
  auto mock_source = std::make_unique<MockObjectReadSource>();
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_source, Read).WillOnce([](void* buf, std::size_t n) {
    ReadSourceResult result{0, HttpResponse{200, {}, {}}};
    result.hashes.md5 = "bad";
    return result;
  });
  EXPECT_CALL(*mock_source, IsOpen).WillRepeatedly(Return(false));
  return std::unique_ptr<ObjectReadSource>(std::move(mock_source));
}

TEST_F(GcpBlobStorageClientProviderTest, GetBlobHashMismatchFails) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName1)))
      .WillOnce(Return(ByMove(BuildBadHashReadResponse())));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_DATA_LOSS)));
    EXPECT_THAT(context.response, IsNull());

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderTest, GetBlobNotFound) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(kBlobName1);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobName1)))
      .WillOnce(
          Return(ByMove(Status(CloudStatusCode::kNotFound, "Blob not found"))));

  get_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(SC_GCP_NOT_FOUND)));
    EXPECT_THAT(context.response, IsNull());

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderTest, GetBlobEmpty) {
  get_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  get_blob_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobNameEmpty);

  EXPECT_CALL(*mock_client_,
              ReadObject(ReadObjectRequestEqual(kBucketName, kBlobNameEmpty)))
      .WillOnce(Return(ByMove(BuildReadResponseFromString(std::string()))));

  get_blob_context_.callback = [this](auto& context) {
    ASSERT_SUCCESS(context.result);

    Blob expected_blob;
    expected_blob.mutable_metadata()->set_bucket_name(kBucketName);
    expected_blob.mutable_metadata()->set_blob_name(kBlobNameEmpty);

    ASSERT_THAT(context.response, testing::NotNull());
    EXPECT_THAT(context.response->blob(), BlobEquals(expected_blob));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->GetBlob(get_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

///////////// ListBlobs ///////////////////////////////////////////////////////

// Matches a ListObjectsRequest with bucket_name and no Prefix.
// Ensures that MaxResults is present and is 1000.
// Ensures StartOffset is not present.
MATCHER_P(ListObjectsRequestEqualNoOffset, bucket_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template GetOption<Prefix>().has_value()) {
    *result_listener
        << "Expected arg to not have a present Prefix value but has: "
        << arg.template GetOption<Prefix>().value();
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(1000),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template HasOption<StartOffset>()) {
    if (auto offset = arg.template GetOption<StartOffset>();
        !offset.value().empty()) {
      *result_listener
          << "Expected ListObjectsRequest to not have StartOffset but has: "
          << offset.value();
      equal = false;
    }
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsNoPrefix) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);

  EXPECT_CALL(*mock_client_,
              ListObjects(ListObjectsRequestEqualNoOffset(kBucketName)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::Substitute(R"""({
            "items": [
              {
                "name": "$0"
              },
              {
                "name": "$1"
              }
            ]
          })""",
                           kBlobName1, kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    ASSERT_SUCCESS(context.result);

    ASSERT_THAT(context.response, testing::NotNull());

    BlobMetadata expected_metadata1, expected_metadata2;
    expected_metadata1.set_bucket_name(kBucketName);
    expected_metadata1.set_blob_name(kBlobName1);
    expected_metadata2.set_bucket_name(kBucketName);
    expected_metadata2.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata1),
                            BlobMetadataEquals(expected_metadata2)));
    EXPECT_FALSE(context.response->has_next_page_token());

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->ListBlobsMetadata(list_blobs_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

// Matches a ListObjectsRequest with bucket_name and Prefix(blob_name).
// Ensures that MaxResults is present and is max_results.
// Ensures StartOffset is not present.
MATCHER_P3(ListObjectsRequestEqualNoOffset, bucket_name, blob_name, max_results,
           "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<Prefix>() ||
      !ExplainMatchResult(Eq(blob_name),
                          arg.template GetOption<Prefix>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(max_results),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (arg.template HasOption<StartOffset>()) {
    if (auto offset = arg.template GetOption<StartOffset>();
        !offset.value().empty()) {
      *result_listener
          << "Expected ListObjectsRequest to not have StartOffset but has: "
          << offset.value();
      equal = false;
    }
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsWithPrefix) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualNoOffset(
                                 kBucketName, "blob_", kDefaultMaxPageSize)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::Substitute(R"""({
            "items": [
              {
                "name": "$0"
              },
              {
                "name": "$1"
              }
            ]
          })""",
                           kBlobName1, kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    ASSERT_SUCCESS(context.result);

    ASSERT_THAT(context.response, testing::NotNull());

    BlobMetadata expected_metadata1, expected_metadata2;
    expected_metadata1.set_bucket_name(kBucketName);
    expected_metadata1.set_blob_name(kBlobName1);
    expected_metadata2.set_bucket_name(kBucketName);
    expected_metadata2.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata1),
                            BlobMetadataEquals(expected_metadata2)));
    EXPECT_FALSE(context.response->has_next_page_token());

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->ListBlobsMetadata(list_blobs_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

// Matches a ListObjectsRequest with bucket_name and blob_name.
// Ensures that MaxResults is present and is max_results.
// Ensures StartOffset is present and is offset.
MATCHER_P4(ListObjectsRequestEqualWithOffset, bucket_name, blob_name,
           max_results, offset, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<Prefix>() ||
      !ExplainMatchResult(Eq(blob_name),
                          arg.template GetOption<Prefix>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MaxResults>() ||
      !ExplainMatchResult(Eq(max_results),
                          arg.template GetOption<MaxResults>().value(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<StartOffset>() ||
      !ExplainMatchResult(Eq(offset),
                          arg.template GetOption<StartOffset>().value(),
                          result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsWithMarker) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");
  list_blobs_context_.request->set_page_token(kBlobName1);

  EXPECT_CALL(*mock_client_,
              ListObjects(ListObjectsRequestEqualWithOffset(
                  kBucketName, "blob_", kDefaultMaxPageSize, kBlobName1)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::Substitute(R"""({
            "items": [
              {
                "name": "$0"
              }
            ]
          })""",
                           kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    ASSERT_SUCCESS(context.result);

    ASSERT_THAT(context.response, testing::NotNull());

    BlobMetadata expected_metadata;
    expected_metadata.set_bucket_name(kBucketName);
    expected_metadata.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata)));
    EXPECT_FALSE(context.response->has_next_page_token());

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->ListBlobsMetadata(list_blobs_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsWithMarkerSkipsFirstObject) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");
  list_blobs_context_.request->set_page_token(kBlobName1);

  EXPECT_CALL(*mock_client_,
              ListObjects(ListObjectsRequestEqualWithOffset(
                  kBucketName, "blob_", kDefaultMaxPageSize, kBlobName1)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::Substitute(R"""({
            "items": [
              {
                "name": "$0"
              },
              {
                "name": "$1"
              }
            ]
          })""",
                           kBlobName1, kBlobName2)))));

  list_blobs_context_.callback = [this](auto& context) {
    ASSERT_SUCCESS(context.result);

    ASSERT_THAT(context.response, testing::NotNull());

    BlobMetadata expected_metadata;
    expected_metadata.set_bucket_name(kBucketName);
    expected_metadata.set_blob_name(kBlobName2);

    EXPECT_THAT(context.response->blob_metadatas(),
                ElementsAre(BlobMetadataEquals(expected_metadata)));
    EXPECT_FALSE(context.response->has_next_page_token());

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->ListBlobsMetadata(list_blobs_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

// Used for Pointwise matching of Blob Metadata -> Blob Metadatas using the
// MATCHER_P version.
MATCHER(BlobMetadatasEqual, "") {
  const auto& actual_metadata = std::get<0>(arg);
  const auto& expected_metadata = std::get<1>(arg);
  return ExplainMatchResult(BlobMetadataEquals(expected_metadata),
                            actual_metadata, result_listener);
}

TEST_F(GcpBlobStorageClientProviderTest,
       ListBlobsReturnsMarkerAndEnforcesPageSize) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  const auto page_size = 100;
  list_blobs_context_.request->set_max_page_size(page_size);

  // Make a JSON object with items named 1 to page_size + 5.
  std::string items_str;
  for (int64_t i = 1; i <= page_size + 5; i++) {
    if (!items_str.empty()) {
      absl::StrAppend(&items_str, ",");
    }
    absl::StrAppend(&items_str, absl::Substitute(R"""({"name": "$0"})""",
                                                 absl::StrCat("blob_", i)));
  }

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualNoOffset(
                                 kBucketName, "blob_", page_size)))
      .WillOnce(Return(ByMove(ListObjectsResponse::FromHttpResponse(
          absl::Substitute(R"""({"items": [$0]})""", items_str)))));

  list_blobs_context_.callback = [this](auto& context) {
    ASSERT_SUCCESS(context.result);

    ASSERT_THAT(context.response, testing::NotNull());

    // We expect to only see blobs 1-100, not [101, 105].
    std::vector<BlobMetadata> expected_blobs;
    expected_blobs.reserve(page_size);
    for (int64_t i = 1; i <= page_size; i++) {
      BlobMetadata metadata;
      metadata.set_bucket_name(kBucketName);
      metadata.set_blob_name(absl::StrCat("blob_", i));
      expected_blobs.push_back(std::move(metadata));
    }
    EXPECT_THAT(context.response->blob_metadatas(),
                Pointwise(BlobMetadatasEqual(), expected_blobs));
    EXPECT_THAT(context.response->next_page_token(), "blob_100");

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->ListBlobsMetadata(list_blobs_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderTest, ListBlobsPropagatesFailure) {
  list_blobs_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  list_blobs_context_.request->mutable_blob_metadata()->set_blob_name("blob_");

  EXPECT_CALL(*mock_client_, ListObjects(ListObjectsRequestEqualNoOffset(
                                 kBucketName, "blob_", kDefaultMaxPageSize)))
      .WillOnce(
          Return(ByMove(Status(CloudStatusCode::kInvalidArgument, "error"))));

  list_blobs_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(
      gcp_cloud_storage_client_->ListBlobsMetadata(list_blobs_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

///////////// PutBlob /////////////////////////////////////////////////////////

MATCHER_P(InsertObjectRequestEquals, expected_request, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(expected_request.bucket_name()), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(expected_request.object_name()), arg.object_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(expected_request.payload()), arg.payload(),
                          result_listener)) {
    equal = false;
  }
  if (!arg.template HasOption<MD5HashValue>() ||
      !ExplainMatchResult(
          Eq(expected_request.template GetOption<MD5HashValue>().value()),
          arg.template GetOption<MD5HashValue>().value(), result_listener)) {
    *result_listener << "Expected arg has the same MD5 but does not.";
    equal = false;
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, PutBlob) {
  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);

  std::string bytes_str = "put_string";
  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  // Use Google Cloud's MD5 method.
  std::string expected_md5_hash =
      google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(std::string{kBucketName},
                                            std::string{kBlobName1}, bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_client_,
              InsertObjectMedia(InsertObjectRequestEquals(expected_request)))
      .WillOnce(Return(ObjectMetadata()));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->PutBlob(put_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST_F(GcpBlobStorageClientProviderTest, PutBlobPropagatesFailure) {
  put_blob_context_.request->mutable_blob()
      ->mutable_metadata()
      ->set_bucket_name(kBucketName);
  put_blob_context_.request->mutable_blob()->mutable_metadata()->set_blob_name(
      kBlobName1);

  std::string bytes_str = "put_string";
  put_blob_context_.request->mutable_blob()->set_data(bytes_str);

  // Use Google Cloud's MD5 method.
  std::string expected_md5_hash =
      google::cloud::storage::ComputeMD5Hash(bytes_str);

  InsertObjectMediaRequest expected_request(std::string{kBucketName},
                                            std::string{kBlobName1}, bytes_str);
  expected_request.set_option(MD5HashValue(expected_md5_hash));

  EXPECT_CALL(*mock_client_,
              InsertObjectMedia(InsertObjectRequestEquals(expected_request)))
      .WillOnce(Return(Status(CloudStatusCode::kInvalidArgument, "failure")));

  put_blob_context_.callback = [this](auto& context) {
    EXPECT_THAT(context.result,
                ResultIs(FailureExecutionResult(
                    SC_BLOB_STORAGE_PROVIDER_UNRETRIABLE_ERROR)));

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->PutBlob(put_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

///////////// DeleteBlob //////////////////////////////////////////////////////

MATCHER_P2(DeleteObjectRequestEquals, bucket_name, blob_name, "") {
  bool equal = true;
  if (!ExplainMatchResult(Eq(bucket_name), arg.bucket_name(),
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Eq(blob_name), arg.object_name(), result_listener)) {
    equal = false;
  }
  return equal;
}

TEST_F(GcpBlobStorageClientProviderTest, DeleteBlob) {
  delete_blob_context_.request->mutable_blob_metadata()->set_bucket_name(
      kBucketName);
  delete_blob_context_.request->mutable_blob_metadata()->set_blob_name(
      kBlobName1);

  EXPECT_CALL(*mock_client_,
              DeleteObject(DeleteObjectRequestEquals(kBucketName, kBlobName1)))
      .WillOnce(Return(EmptyResponse{}));

  delete_blob_context_.callback = [this](auto& context) {
    EXPECT_SUCCESS(context.result);

    absl::MutexLock lock(&finish_called_mu_);
    finish_called_ = true;
  };

  EXPECT_TRUE(gcp_cloud_storage_client_->DeleteBlob(delete_blob_context_).ok());

  absl::MutexLock lock(&finish_called_mu_);
  finish_called_mu_.Await(absl::Condition(&finish_called_));
}

TEST(GcpBlobStorageClientProviderTestII, InitFailedToFetchProjectId) {
  MockAsyncExecutor async_executor_mock;
  MockAsyncExecutor io_async_executor_mock;
  MockInstanceClientProvider instance_client_mock;
  instance_client_mock.get_instance_resource_name_mock = absl::UnknownError("");

  // Empty project_id set.
  GcpBlobStorageClientProvider client(
      BlobStorageClientOptions(), &instance_client_mock, &async_executor_mock,
      &io_async_executor_mock,
      std::make_unique<NiceMock<MockGcpCloudStorageFactory>>());
  EXPECT_FALSE(client.Init().ok());
}

}  // namespace google::scp::cpio::client_providers::test
