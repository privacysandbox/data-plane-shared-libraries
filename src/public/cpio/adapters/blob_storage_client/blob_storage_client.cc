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
#include <string_view>
#include <utility>

#include "src/core/common/global_logger/global_logger.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/errors.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"
#include "src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"

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
using google::scp::core::ExecutionResult;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::cpio::client_providers::BlobStorageClientProviderFactory;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;

namespace {
constexpr std::string_view kBlobStorageClient = "BlobStorageClient";
}  // namespace

namespace google::scp::cpio {

ExecutionResult BlobStorageClient::Init() noexcept {
  cpio_ = &GlobalCpio::GetGlobalCpio();
  AsyncExecutorInterface* cpu_async_executor;
  if (auto executor = cpio_->GetCpuAsyncExecutor(); !executor.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kBlobStorageClient, kZeroUuid, execution_result,
              "Failed to get AsyncExecutor.");
    return execution_result;
  } else {
    cpu_async_executor = *executor;
  }

  AsyncExecutorInterface* io_async_executor;
  if (auto executor = cpio_->GetIoAsyncExecutor(); !executor.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kBlobStorageClient, kZeroUuid, execution_result,
              "Failed to get IOAsyncExecutor.");
    return execution_result;
  } else {
    io_async_executor = *executor;
  }

  InstanceClientProviderInterface* instance_client;
  if (auto client = cpio_->GetInstanceClientProvider(); !client.ok()) {
    ExecutionResult execution_result;
    SCP_ERROR(kBlobStorageClient, kZeroUuid, execution_result,
              "Failed to get InstanceClientProvider.");
    return execution_result;
  } else {
    instance_client = *client;
  }
  BlobStorageClientOptions options = *options_;
  if (options.project_id.empty()) {
    options.project_id = cpio_->GetProjectId();
  }
  if (options.region.empty()) {
    options.region = cpio_->GetRegion();
  }
  blob_storage_client_provider_ = BlobStorageClientProviderFactory::Create(
      std::move(options), instance_client, cpu_async_executor,
      io_async_executor);
  auto execution_result = blob_storage_client_provider_->Init();
  if (!execution_result.Successful()) {
    SCP_ERROR(kBlobStorageClient, kZeroUuid, execution_result,
              "Failed to initialize BlobStorageClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult BlobStorageClient::Run() noexcept {
  auto execution_result = blob_storage_client_provider_->Run();
  if (!execution_result.Successful()) {
    SCP_ERROR(kBlobStorageClient, kZeroUuid, execution_result,
              "Failed to run BlobStorageClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult BlobStorageClient::Stop() noexcept {
  auto execution_result = blob_storage_client_provider_->Stop();
  if (!execution_result.Successful()) {
    SCP_ERROR(kBlobStorageClient, kZeroUuid, execution_result,
              "Failed to stop BlobStorageClientProvider.");
    return execution_result;
  }
  return SuccessExecutionResult();
}

ExecutionResult BlobStorageClient::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context) noexcept {
  return blob_storage_client_provider_->GetBlob(get_blob_context);
}

ExecutionResult BlobStorageClient::ListBlobsMetadata(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
        list_blobs_metadata_context) noexcept {
  return blob_storage_client_provider_->ListBlobsMetadata(
      list_blobs_metadata_context);
}

ExecutionResult BlobStorageClient::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context) noexcept {
  return blob_storage_client_provider_->PutBlob(put_blob_context);
}

ExecutionResult BlobStorageClient::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>
        delete_blob_context) noexcept {
  return blob_storage_client_provider_->DeleteBlob(delete_blob_context);
}

ExecutionResult BlobStorageClient::GetBlobStream(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
        get_blob_stream_context) noexcept {
  return blob_storage_client_provider_->GetBlobStream(get_blob_stream_context);
}

ExecutionResult BlobStorageClient::PutBlobStream(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        put_blob_stream_context) noexcept {
  return blob_storage_client_provider_->PutBlobStream(put_blob_stream_context);
}

std::unique_ptr<BlobStorageClientInterface> BlobStorageClientFactory::Create(
    BlobStorageClientOptions options) {
  return std::make_unique<BlobStorageClient>(
      std::make_shared<BlobStorageClientOptions>(std::move(options)));
}
}  // namespace google::scp::cpio
