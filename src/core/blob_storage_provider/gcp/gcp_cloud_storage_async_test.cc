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

#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/synchronization/notification.h"
#include "google/cloud/status.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/internal/object_requests.h"
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/mock/mock_async_executor.h"
#include "src/core/blob_storage_provider/common/error_codes.h"
#include "src/core/blob_storage_provider/gcp/gcp_cloud_storage.h"
#include "src/core/config_provider/mock/mock_config_provider.h"
#include "src/core/interface/blob_storage_provider_interface.h"
#include "src/core/interface/configuration_keys.h"
#include "src/core/interface/type_def.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/hashing.h"
#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::core::test {
namespace {

using google::cloud::Options;
using google::cloud::storage::BucketLifecycle;
using google::cloud::storage::BucketMetadata;
using google::cloud::storage::Client;
using google::cloud::storage::LifecycleRule;
using google::cloud::storage::Prefix;
using google::cloud::storage::ProjectIdOption;
using google::cloud::storage::StartOffset;
using google::cloud::storage::internal::EmptyResponse;
using google::cloud::storage::internal::HttpResponse;
using google::scp::core::AsyncExecutor;
using google::scp::core::DeleteBlobRequest;
using google::scp::core::DeleteBlobResponse;
using google::scp::core::GetBlobRequest;
using google::scp::core::GetBlobResponse;
using google::scp::core::ListBlobsRequest;
using google::scp::core::ListBlobsResponse;
using google::scp::core::PutBlobRequest;
using google::scp::core::PutBlobResponse;
using google::scp::core::async_executor::mock::MockAsyncExecutor;
using google::scp::core::blob_storage_provider::GcpCloudStorageClient;
using google::scp::core::blob_storage_provider::GcpCloudStorageProvider;
using google::scp::core::config_provider::mock::MockConfigProvider;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::utils::Base64Encode;
using testing::ElementsAreArray;
using testing::Eq;
using testing::FieldsAre;
using testing::IsNull;
using testing::NotNull;
using testing::Pointee;
using testing::Pointwise;
using testing::StrEq;

constexpr std::string_view kProject = "admcloud-coordinator1";
constexpr std::string_view kBucketName = "test-bucket";
constexpr std::string_view kDefaultBlobName = "blob";
constexpr std::string_view kDefaultBlobValue = "default_value";
constexpr size_t kThreadCount = 5;
constexpr size_t kQueueSize = 225;

class GcpCloudStorageClientAsyncTests : public testing::Test {
 protected:
  static void SetUpTestSuite() {
    client_ = new Client(Options{}.set<ProjectIdOption>(std::string{kProject}));
  }

  static void TearDownTestSuite() {
    auto s = client_->DeleteBucket(std::string{kBucketName});
    EXPECT_TRUE(s.ok()) << s.message();
    delete client_;
  }

  static BucketLifecycle ExpireImmediately() {
    BucketLifecycle lifecycle;
    lifecycle.rule.emplace_back(LifecycleRule::MaxAge(0),
                                LifecycleRule::Delete());
    return lifecycle;
  }

  static void InsertDefaultBlob() {
    auto buckets_list = client_->ListBuckets();
    if (std::none_of(
            buckets_list.begin(), buckets_list.end(),
            [](const auto& bucket) { return bucket->name() == kBucketName; })) {
      auto bucket_metadata_or = client_->CreateBucket(
          std::string{kBucketName},
          BucketMetadata().set_lifecycle(ExpireImmediately()));
      EXPECT_TRUE(bucket_metadata_or.ok());
    }

    auto metadata_or = client_->InsertObject(std::string{kBucketName},
                                             std::string{kDefaultBlobName},
                                             std::string{kDefaultBlobValue});
    EXPECT_TRUE(metadata_or.ok()) << metadata_or.status().message();
  }

  static void ClearBucket() {
    auto buckets_list = client_->ListBuckets();
    if (std::none_of(
            buckets_list.begin(), buckets_list.end(),
            [](const auto& bucket) { return bucket->name() == kBucketName; })) {
      return;
    }

    for (const auto& obj_metadata :
         client_->ListObjects(std::string{kBucketName})) {
      ASSERT_TRUE(obj_metadata.ok()) << obj_metadata.status().message();
      auto status =
          client_->DeleteObject(std::string{kBucketName}, obj_metadata->name());
      ASSERT_TRUE(status.ok()) << status.message();
    }
  }

  GcpCloudStorageClientAsyncTests()
      : async_executor_(
            std::make_shared<AsyncExecutor>(kThreadCount, kQueueSize)),
        io_async_executor_(
            std::make_shared<AsyncExecutor>(kThreadCount, kQueueSize)),
        config_provider_(std::make_shared<MockConfigProvider>()) {
    config_provider_->Set(std::string(kGcpProjectId), std::string(kProject));
    GcpCloudStorageProvider provider(async_executor_, io_async_executor_,
                                     config_provider_, AsyncPriority::Normal,
                                     AsyncPriority::Normal);
    if (!provider.Init().Successful()) throw std::runtime_error("Error Init");
    if (!provider.Run().Successful()) throw std::runtime_error("Error Run");
    provider.CreateBlobStorageClient(gcp_cloud_storage_client_);

    ClearBucket();
    InsertDefaultBlob();
  }

  ~GcpCloudStorageClientAsyncTests() { ClearBucket(); }

  std::shared_ptr<AsyncExecutor> async_executor_, io_async_executor_;
  std::shared_ptr<MockConfigProvider> config_provider_;
  std::shared_ptr<BlobStorageClientInterface> gcp_cloud_storage_client_;

  static Client* client_;
};

Client* GcpCloudStorageClientAsyncTests::client_;

TEST_F(GcpCloudStorageClientAsyncTests, SimpleGetTest) {
  absl::Notification finished;
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context;
  get_blob_context.request = std::make_shared<GetBlobRequest>(
      GetBlobRequest{std::make_shared<std::string>(kBucketName),
                     std::make_shared<std::string>(kDefaultBlobName)});

  get_blob_context.callback = [&finished](auto& context) {
    const auto& response = context.response;
    EXPECT_THAT(response, NotNull());
    EXPECT_THAT(response->buffer, NotNull());

    EXPECT_THAT(*response->buffer, StrEq(kDefaultBlobValue));
    finished.Notify();
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_->GetBlob(get_blob_context));

  finished.WaitForNotification();
}

MATCHER_P(BlobsEqual, expected, "") {
  bool equal = true;
  if (!ExplainMatchResult(Pointee(Eq(*expected.bucket_name)), arg.bucket_name,
                          result_listener)) {
    equal = false;
  }
  if (!ExplainMatchResult(Pointee(Eq(*expected.blob_name)), arg.blob_name,
                          result_listener)) {
    equal = false;
  }
  return equal;
}

MATCHER(BlobsEqual, "") {
  const auto& actual = std::get<0>(arg);
  const auto& expected = std::get<1>(arg);
  return ExplainMatchResult(BlobsEqual(expected), actual, result_listener);
}

TEST_F(GcpCloudStorageClientAsyncTests, ListBlobsTest) {
  // Delete the default blob that is inserted.
  ClearBucket();
  constexpr int64_t kPageSize = 1000, kAdditionalBlobCount = 5;
  // Insert more objects than will fit in one page.
  // For blobs starting with name suffix "1000" and after i.e. [blob_1000,
  // blob_1001...], they move to the first page right after "blob_10" in
  // lexicographical order.
  for (auto i = 1; i <= (kPageSize + kAdditionalBlobCount); i++) {
    auto s = client_->InsertObject(std::string{kBucketName},
                                   absl::StrCat("blob_", i),
                                   absl::StrCat("value_", i));
    ASSERT_TRUE(s.ok()) << s.status().message();
  }

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context;
  list_blobs_context.request = std::make_shared<ListBlobsRequest>(
      ListBlobsRequest{{std::make_shared<std::string>(kBucketName)}});
  std::shared_ptr<std::string> next_marker;
  {
    absl::Notification finished;

    list_blobs_context.callback = [&finished, &next_marker,
                                   kPageSize](auto& context) {
      ASSERT_SUCCESS(context.result);

      EXPECT_THAT(context.response, NotNull());

      // We must add all blobs including the additional that won't be present so
      // we can sort them lexicographically.
      std::vector<Blob> expected_blobs;
      expected_blobs.reserve(kPageSize + kAdditionalBlobCount);
      for (auto i = 1; i <= (kPageSize + kAdditionalBlobCount); i++) {
        expected_blobs.push_back(
            Blob{std::make_shared<std::string>(kBucketName),
                 std::make_shared<std::string>(absl::StrCat("blob_", i))});
      }
      std::sort(expected_blobs.begin(), expected_blobs.end(),
                [](const Blob& l, const Blob& r) {
                  return *l.blob_name < *r.blob_name;
                });
      // Remove the last additional elements from the list as they are not
      // included in the first page.
      expected_blobs.erase(expected_blobs.end() - kAdditionalBlobCount,
                           expected_blobs.end());

      EXPECT_THAT(context.response->blobs,
                  Pointee(Pointwise(BlobsEqual(), expected_blobs)));
      if (context.response->blobs) {
        // Sanity checks.
        EXPECT_THAT(context.response->blobs->back().blob_name,
                    Pointee(Eq("blob_994")));
        EXPECT_EQ(expected_blobs.size(), kPageSize);
        EXPECT_EQ(context.response->blobs->size(), expected_blobs.size());
      }
      EXPECT_THAT(context.response->next_marker,
                  Pointee(FieldsAre(Pointee(Eq(kBucketName)),
                                    Pointee(Eq("blob_994")))));

      next_marker = context.response->next_marker->blob_name;
      finished.Notify();
    };

    ASSERT_THAT(gcp_cloud_storage_client_->ListBlobs(list_blobs_context),
                IsSuccessful());

    finished.WaitForNotification();
  }

  {
    absl::Notification finished;

    // Inherit the next_marker from the response to test that chained calls work
    // correctly.
    list_blobs_context.request->marker = next_marker;
    list_blobs_context.callback = [&finished](auto& context) {
      ASSERT_SUCCESS(context.result);

      EXPECT_THAT(context.response, NotNull());

      std::vector<Blob> expected_blobs;
      // We expect to find blobs 995->999 if sorting lexicographically.
      for (auto i = kPageSize - kAdditionalBlobCount; i < kPageSize; i++) {
        expected_blobs.push_back(
            Blob{std::make_shared<std::string>(kBucketName),
                 std::make_shared<std::string>(absl::StrCat("blob_", i))});
      }

      EXPECT_THAT(context.response->blobs,
                  Pointee(Pointwise(BlobsEqual(), expected_blobs)));
      EXPECT_THAT(context.response->next_marker, IsNull());
      finished.Notify();
    };

    ASSERT_THAT(gcp_cloud_storage_client_->ListBlobs(list_blobs_context),
                IsSuccessful());

    finished.WaitForNotification();
  }
}

TEST_F(GcpCloudStorageClientAsyncTests, SimplePutTest) {
  absl::Notification finished;
  std::string new_blob_val("some new value");
  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context;
  put_blob_context.request = std::make_shared<PutBlobRequest>(
      PutBlobRequest{{std::make_shared<std::string>(kBucketName),
                      std::make_shared<std::string>(kDefaultBlobName)}});
  put_blob_context.request->buffer =
      std::make_shared<std::string>(new_blob_val);

  put_blob_context.callback = [&finished, &new_blob_val](auto& context) {
    ASSERT_TRUE(context.result.Successful());

    auto object_read_stream = client_->ReadObject(
        std::string{kBucketName}, std::string{kDefaultBlobName});
    ASSERT_TRUE(object_read_stream && !object_read_stream.bad());

    std::string buffer(new_blob_val.size(), '\0');
    object_read_stream.read(buffer.data(), *object_read_stream.size());

    EXPECT_THAT(context.request->buffer, NotNull());
    EXPECT_THAT(buffer, StrEq(*context.request->buffer));
    finished.Notify();
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_->PutBlob(put_blob_context));

  finished.WaitForNotification();
}

TEST_F(GcpCloudStorageClientAsyncTests, SimpleDeleteTest) {
  absl::Notification finished;
  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context;
  delete_blob_context.request = std::make_shared<DeleteBlobRequest>(
      DeleteBlobRequest{std::make_shared<std::string>(kBucketName),
                        std::make_shared<std::string>(kDefaultBlobName)});

  delete_blob_context.callback = [&finished](auto) {
    auto objects_reader = client_->ListObjects(std::string{kBucketName});
    ASSERT_EQ(objects_reader.begin(), objects_reader.end());
    finished.Notify();
  };

  EXPECT_SUCCESS(gcp_cloud_storage_client_->DeleteBlob(delete_blob_context));

  finished.WaitForNotification();

  // Test.
}

}  // namespace
}  // namespace google::scp::core::test
