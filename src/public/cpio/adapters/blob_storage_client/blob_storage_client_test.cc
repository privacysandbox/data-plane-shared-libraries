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

#include "src/public/cpio/adapters/blob_storage_client/blob_storage_client.h"

#include <gtest/gtest.h>

#include "absl/log/check.h"
#include "src/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/adapters/blob_storage_client/mock_blob_storage_client_with_overrides.h"
#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

using google::cmrt::sdk::blob_storage_service::v1::BlobMetadata;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest;
using google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::test::IsSuccessful;
using google::scp::cpio::mock::MockBlobStorageClientWithOverrides;
using testing::Return;

namespace google::scp::cpio::test {

class BlobStorageClientTest : public ::testing::Test {
 protected:
  BlobStorageClientTest() {
    CHECK_OK(client_.Init()) << "client_ initialization unsuccessful";
  }

  MockBlobStorageClientWithOverrides client_;
};

TEST_F(BlobStorageClientTest, GetBlobSuccess) {
  AsyncContext<GetBlobRequest, GetBlobResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlob)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.GetBlob(context).ok());
}

TEST_F(BlobStorageClientTest, ListBlobsMetadataSuccess) {
  AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), ListBlobsMetadata)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.ListBlobsMetadata(context).ok());
}

TEST_F(BlobStorageClientTest, PutBlobSuccess) {
  AsyncContext<PutBlobRequest, PutBlobResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlob)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.PutBlob(context).ok());
}

TEST_F(BlobStorageClientTest, DeleteBlobSuccess) {
  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), DeleteBlob)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.DeleteBlob(context).ok());
}

TEST_F(BlobStorageClientTest, GetBlobStreamSuccess) {
  ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), GetBlobStream)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.GetBlobStream(context).ok());
}

TEST_F(BlobStorageClientTest, PutBlobStreamSuccess) {
  ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse> context;
  EXPECT_CALL(client_.GetBlobStorageClientProvider(), PutBlobStream)
      .WillOnce(Return(absl::OkStatus()));
  EXPECT_TRUE(client_.PutBlobStream(context).ok());
}

}  // namespace google::scp::cpio::test
