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

#include "blob_storage_client.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"
#include "src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#include "src/util/status_macro/status_macros.h"

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
using google::scp::cpio::client_providers::BlobStorageClientProviderFactory;
using google::scp::cpio::client_providers::GlobalCpio;

namespace google::scp::cpio {

absl::Status BlobStorageClient::Init() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  BlobStorageClientOptions options = options_;
  if (options.project_id.empty()) {
    options.project_id = cpio_->GetProjectId();
  }
  if (options.region.empty()) {
    options.region = cpio_->GetRegion();
  }
  PS_ASSIGN_OR_RETURN(
      blob_storage_client_provider_,
      BlobStorageClientProviderFactory::Create(
          std::move(options), &cpio_->GetInstanceClientProvider(),
          &cpio_->GetCpuAsyncExecutor(), &cpio_->GetIoAsyncExecutor()));
  return absl::OkStatus();
}

absl::Status BlobStorageClient::Run() noexcept { return absl::OkStatus(); }

absl::Status BlobStorageClient::Stop() noexcept { return absl::OkStatus(); }

absl::Status BlobStorageClient::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context) noexcept {
  return blob_storage_client_provider_->GetBlob(get_blob_context);
}

absl::Status BlobStorageClient::ListBlobsMetadata(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
        list_blobs_metadata_context) noexcept {
  return blob_storage_client_provider_->ListBlobsMetadata(
      list_blobs_metadata_context);
}

absl::Status BlobStorageClient::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context) noexcept {
  return blob_storage_client_provider_->PutBlob(put_blob_context);
}

absl::Status BlobStorageClient::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>
        delete_blob_context) noexcept {
  return blob_storage_client_provider_->DeleteBlob(delete_blob_context);
}

absl::Status BlobStorageClient::GetBlobStream(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
        get_blob_stream_context) noexcept {
  return blob_storage_client_provider_->GetBlobStream(get_blob_stream_context);
}

absl::Status BlobStorageClient::PutBlobStream(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        put_blob_stream_context) noexcept {
  return blob_storage_client_provider_->PutBlobStream(put_blob_stream_context);
}

std::unique_ptr<BlobStorageClientInterface> BlobStorageClientFactory::Create(
    BlobStorageClientOptions options) {
  return std::make_unique<BlobStorageClient>(std::move(options));
}
}  // namespace google::scp::cpio
