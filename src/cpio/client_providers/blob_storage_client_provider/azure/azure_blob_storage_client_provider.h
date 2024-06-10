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

#ifndef CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_AZURE_AZURE_BLOB_STORAGE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_AZURE_AZURE_BLOB_STORAGE_CLIENT_PROVIDER_H_

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/config_provider_interface.h"
#include "src/core/interface/streaming_context.h"
#include "src/cpio/client_providers/blob_storage_client_provider/common/error_codes.h"
#include "src/cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"

namespace google::scp::cpio::client_providers {

/*! @copydoc BlobStorageClientProviderInterface
 */
class AzureBlobStorageClientProvider
    : public BlobStorageClientProviderInterface {
 public:
  explicit AzureBlobStorageClientProvider(
      BlobStorageClientOptions options,
      InstanceClientProviderInterface* instance_client,
      core::AsyncExecutorInterface* cpu_async_executor,
      core::AsyncExecutorInterface* io_async_executor) {}

  absl::Status GetBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&
          get_blob_context) noexcept override;

  absl::Status GetBlobStream(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          get_blob_stream_context) noexcept override;

  absl::Status ListBlobsMetadata(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&
          list_blobs_context) noexcept override;

  absl::Status PutBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&
          put_blob_context) noexcept override;

  absl::Status PutBlobStream(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context) noexcept override;

  absl::Status DeleteBlob(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&
          delete_blob_context) noexcept override;

 private:
  static constexpr std::string_view kAzureBlobStorageClientProvider =
      "AzureBlobStorageClientProvider";
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_AZURE_AZURE_BLOB_STORAGE_CLIENT_PROVIDER_H_
