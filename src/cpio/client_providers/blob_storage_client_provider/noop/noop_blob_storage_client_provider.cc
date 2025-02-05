/*
 * Copyright 2025 Google LLC
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

#include <memory>

#include "absl/base/nullability.h"
#include "src/core/interface/async_context.h"
#include "src/cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

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
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ProducerStreamingContext;
using google::scp::cpio::client_providers::BlobStorageClientProviderInterface;

namespace {
class NoopBlobStorageClientProvider
    : public BlobStorageClientProviderInterface {
 public:
  absl::Status GetBlob(AsyncContext<GetBlobRequest, GetBlobResponse>&
                       /* get_blob_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status GetBlobStream(
      ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>&
      /* get_blob_stream_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status ListBlobsMetadata(
      AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>&
      /* list_blobs_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status PutBlob(AsyncContext<PutBlobRequest, PutBlobResponse>&
                       /* put_blob_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status PutBlobStream(
      ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>&
      /* put_blob_stream_context */) noexcept override {
    return absl::UnimplementedError("");
  }

  absl::Status DeleteBlob(AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
                          /* delete_blob_context */) noexcept override {
    return absl::UnimplementedError("");
  }
};
}  // namespace

namespace google::scp::cpio::client_providers {
absl::StatusOr<std::unique_ptr<BlobStorageClientProviderInterface>>
BlobStorageClientProviderFactory::Create(
    BlobStorageClientOptions /* options */,
    absl::Nonnull<InstanceClientProviderInterface*> /* instance_client */,
    absl::Nonnull<AsyncExecutorInterface*> /* cpu_async_executor */,
    absl::Nonnull<AsyncExecutorInterface*> /* io_async_executor */) noexcept {
  return std::make_unique<NoopBlobStorageClientProvider>();
}
}  // namespace google::scp::cpio::client_providers
