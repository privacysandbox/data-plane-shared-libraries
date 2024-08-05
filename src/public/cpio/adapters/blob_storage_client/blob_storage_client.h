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

#ifndef PUBLIC_CPIO_ADAPTERS_BLOB_STORAGE_CLIENT_BLOB_STORAGE_CLIENT_H_
#define PUBLIC_CPIO_ADAPTERS_BLOB_STORAGE_CLIENT_BLOB_STORAGE_CLIENT_H_

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"
#include "src/public/cpio/interface/type_def.h"
#include "src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

namespace google::scp::cpio {
/*! @copydoc BlobStorageClientInterface
 */
class BlobStorageClient : public BlobStorageClientInterface {
 public:
  explicit BlobStorageClient(BlobStorageClientOptions options)
      : options_(std::move(options)) {}

  virtual ~BlobStorageClient() = default;

  absl::Status Init() noexcept override;
  absl::Status Run() noexcept override;
  absl::Status Stop() noexcept override;

  absl::Status GetBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse>
          get_blob_context) noexcept override;

  absl::Status ListBlobsMetadata(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          google::cmrt::sdk::blob_storage_service::v1::
              ListBlobsMetadataResponse>
          list_blobs_metadata_context) noexcept override;

  absl::Status PutBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobResponse>
          put_blob_context) noexcept override;

  absl::Status DeleteBlob(
      core::AsyncContext<
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          google::cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>
          delete_blob_context) noexcept override;

  /// Streaming operations.

  absl::Status GetBlobStream(
      core::ConsumerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>
          get_blob_stream_context) noexcept override;

  absl::Status PutBlobStream(
      core::ProducerStreamingContext<
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          google::cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>
          put_blob_stream_context) noexcept override;

 protected:
  std::unique_ptr<client_providers::BlobStorageClientProviderInterface>
      blob_storage_client_provider_;

 private:
  BlobStorageClientOptions options_;
  client_providers::CpioProviderInterface* cpio_;
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_BLOB_STORAGE_CLIENT_BLOB_STORAGE_CLIENT_H_
