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

#include <atomic>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "scp/cc/core/blob_storage_provider/mock/mock_blob_storage_provider.h"
#include "scp/cc/public/core/test/interface/execution_result_matchers.h"

using google::scp::core::blob_storage_provider::mock::MockBlobStorageProvider;
using ::testing::StrEq;

namespace google::scp::core::test {
TEST(MockBlobStorageProviderTest, GetBlob) {
  MockBlobStorageProvider mock_blob_storage_provider;
  std::filesystem::current_path("/tmp");

  std::atomic<bool> condition = false;
  std::filesystem::create_directory("bucket_get");

  std::string file_content = "1234";
  std::vector<char> bytes(file_content.begin(), file_content.end());

  std::ofstream output_stream("bucket_get/1.txt",
                              std::ios::trunc | std::ios::binary);
  output_stream << file_content;
  output_stream.close();

  std::shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      std::make_shared<GetBlobRequest>(),
      [&](AsyncContext<GetBlobRequest, GetBlobResponse>& context) {
        ASSERT_SUCCESS(context.result);
        EXPECT_EQ(context.response->buffer->length, 4);
        EXPECT_EQ(context.response->buffer->bytes->size(), bytes.size());

        for (size_t i = 0; i < bytes.size(); ++i) {
          EXPECT_EQ(context.response->buffer->bytes->at(i), bytes[i]);
        }
        condition = true;
      });

  get_blob_context.request->bucket_name =
      std::make_shared<std::string>("bucket_get");
  get_blob_context.request->blob_name = std::make_shared<std::string>("1.txt");
  EXPECT_SUCCESS(blob_storage_client->GetBlob(get_blob_context));
}

TEST(MockBlobStorageProviderTest, PutBlob) {
  MockBlobStorageProvider mock_blob_storage_provider;
  std::filesystem::current_path("/tmp");

  std::atomic<bool> condition = false;
  std::filesystem::create_directory("bucket_put");

  std::string file_content = "1234";
  std::vector<Byte> bytes(file_content.begin(), file_content.end());

  std::shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context(
      std::make_shared<PutBlobRequest>(),
      [&](AsyncContext<PutBlobRequest, PutBlobResponse>& context) {
        ASSERT_SUCCESS(context.result);

        std::ifstream input_stream("bucket_put/test_hash/1.txt",
                                   std::ios::ate | std::ios::binary);
        auto content_length = input_stream.tellg();
        input_stream.seekg(0);
        EXPECT_EQ(bytes.size(), content_length);

        std::vector<char> to_read(content_length);
        input_stream.read(to_read.data(), content_length);

        for (size_t i = 0; i < to_read.size(); ++i) {
          EXPECT_EQ(bytes[i], to_read[i]);
        }
        condition = true;
      });

  put_blob_context.request->bucket_name =
      std::make_shared<std::string>("bucket_put");
  put_blob_context.request->blob_name =
      std::make_shared<std::string>("test_hash/1.txt");
  put_blob_context.request->buffer = std::make_shared<BytesBuffer>();
  put_blob_context.request->buffer->bytes =
      std::make_shared<std::vector<Byte>>(bytes);
  put_blob_context.request->buffer->length = bytes.size();
  EXPECT_SUCCESS(blob_storage_client->PutBlob(put_blob_context));
}

TEST(MockBlobStorageProviderTest, DeleteBlob) {
  MockBlobStorageProvider mock_blob_storage_provider;
  std::filesystem::current_path("/tmp");

  std::atomic<bool> condition = false;
  std::filesystem::create_directory("bucket_delete");

  std::string file_content = "1234";
  std::vector<char> bytes(file_content.begin(), file_content.end());

  std::ofstream output_stream("bucket_delete/2.txt",
                              std::ios::trunc | std::ios::binary);
  output_stream << file_content;
  output_stream.close();

  std::shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<DeleteBlobRequest, DeleteBlobResponse> delete_blob_context(
      std::make_shared<DeleteBlobRequest>(),
      [&](AsyncContext<DeleteBlobRequest, DeleteBlobResponse>& context) {
        ASSERT_SUCCESS(context.result);

        EXPECT_EQ(
            std::distance(std::filesystem::directory_iterator("bucket_delete"),
                          std::filesystem::directory_iterator{}),
            0);
        condition = true;
      });

  delete_blob_context.request->bucket_name =
      std::make_shared<std::string>("bucket_delete");
  delete_blob_context.request->blob_name =
      std::make_shared<std::string>("2.txt");
  EXPECT_SUCCESS(blob_storage_client->DeleteBlob(delete_blob_context));
}

TEST(MockBlobStorageProviderTest, ListBlobs) {
  MockBlobStorageProvider mock_blob_storage_provider;
  std::filesystem::current_path("/tmp");

  std::atomic<bool> condition = false;
  std::filesystem::create_directory("bucket_list");
  std::filesystem::create_directory("bucket_list/1");
  std::filesystem::create_directory("bucket_list/1/3");
  std::filesystem::create_directory("bucket_list/2");

  std::ofstream output_stream1("bucket_list/2.txt",
                               std::ios::trunc | std::ios::binary);
  output_stream1.close();

  std::ofstream output_stream2("bucket_list/1/2.txt",
                               std::ios::trunc | std::ios::binary);
  output_stream2.close();

  std::ofstream output_stream3("bucket_list/1/3/4.txt",
                               std::ios::trunc | std::ios::binary);
  output_stream3.close();

  std::ofstream output_stream4("bucket_list/2/5.txt",
                               std::ios::trunc | std::ios::binary);
  output_stream4.close();

  std::shared_ptr<BlobStorageClientInterface> blob_storage_client;
  EXPECT_SUCCESS(
      mock_blob_storage_provider.CreateBlobStorageClient(blob_storage_client));

  AsyncContext<ListBlobsRequest, ListBlobsResponse> list_blobs_context(
      std::make_shared<ListBlobsRequest>(),
      [&](AsyncContext<ListBlobsRequest, ListBlobsResponse>& context) {
        ASSERT_SUCCESS(context.result);

        constexpr int kNumValues = 7;
        absl::InlinedVector<std::string_view, kNumValues> expected = {
            "1", "1/2.txt", "1/3", "1/3/4.txt", "2", "2.txt", "2/5.txt",
        };

        EXPECT_EQ(context.response->blobs->size(), kNumValues);
        for (int i = 0; i < kNumValues; ++i) {
          EXPECT_THAT(*context.response->blobs->at(i).blob_name,
                      StrEq(expected[i]));
        }

        condition = true;
      });

  list_blobs_context.request->bucket_name =
      std::make_shared<std::string>("bucket_list");
  list_blobs_context.request->blob_name = std::make_shared<std::string>("");
  EXPECT_SUCCESS(blob_storage_client->ListBlobs(list_blobs_context));
}

}  // namespace google::scp::core::test
