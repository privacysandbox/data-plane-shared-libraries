/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_CPIO_MOCK_BLOB_STORAGE_CLIENT_MOCK_BLOB_STORAGE_CLIENT_H_
#define PUBLIC_CPIO_MOCK_BLOB_STORAGE_CLIENT_MOCK_BLOB_STORAGE_CLIENT_H_

#include <gmock/gmock.h>

#include "absl/status/status.h"
#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

namespace google::scp::cpio {
class MockBlobStorageClient : public BlobStorageClientInterface {
 public:
  MockBlobStorageClient() {
    ON_CALL(*this, Init).WillByDefault(testing::Return(absl::OkStatus()));
    ON_CALL(*this, Run).WillByDefault(testing::Return(absl::OkStatus()));
    ON_CALL(*this, Stop).WillByDefault(testing::Return(absl::OkStatus()));
  }

  MOCK_METHOD(absl::Status, Init, (), (noexcept, override));
  MOCK_METHOD(absl::Status, Run, (), (noexcept, override));
  MOCK_METHOD(absl::Status, Stop, (), (noexcept, override));

  MOCK_METHOD(
      absl::Status, GetBlob,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      absl::Status, ListBlobsMetadata,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          google::cmrt::sdk::blob_storage_service::v1::
              ListBlobsMetadataResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      absl::Status, PutBlob,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      absl::Status, DeleteBlob,
      ((core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>)),
      (noexcept, override));

  /// Streaming operations.

  MOCK_METHOD(
      absl::Status, GetBlobStream,
      ((core::ConsumerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>)),
      (noexcept, override));

  MOCK_METHOD(
      absl::Status, PutBlobStream,
      ((core::ProducerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>)),
      (noexcept, override));
};

}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_MOCK_BLOB_STORAGE_CLIENT_MOCK_BLOB_STORAGE_CLIENT_H_
