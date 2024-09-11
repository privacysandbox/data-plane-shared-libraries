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

#include "azure_blob_storage_client_provider.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "src/core/common/global_logger/global_logger.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/blob_storage_provider_interface.h"
#include "src/core/interface/configuration_keys.h"
#include "src/core/interface/type_def.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/hashing.h"
#include "src/cpio/client_providers/blob_storage_client_provider/common/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"

using google::cmrt::sdk::blob_storage_service::v1::Blob;
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
using google::protobuf::util::TimeUtil;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncPriority;
using google::scp::core::ConsumerStreamingContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::ProducerStreamingContext;
using google::scp::core::RetryExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::common::TimeProvider;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED;
using google::scp::core::errors::
    SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED;
using google::scp::core::utils::Base64Encode;

namespace google::scp::cpio::client_providers {

absl::Status AzureBlobStorageClientProvider::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  // Not implemented
  return absl::OkStatus();
}

absl::Status AzureBlobStorageClientProvider::GetBlobStream(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>&
        get_blob_stream_context) noexcept {
  // Not implemented
  return absl::OkStatus();
}

absl::Status AzureBlobStorageClientProvider::ListBlobsMetadata(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>&
        list_blobs_context) noexcept {
  // Not implemented
  return absl::OkStatus();
}

absl::Status AzureBlobStorageClientProvider::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) noexcept {
  // Not implemented
  return absl::OkStatus();
}

absl::Status AzureBlobStorageClientProvider::PutBlobStream(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>&
        put_blob_stream_context) noexcept {
  // Not implemented
  return absl::OkStatus();
}

absl::Status AzureBlobStorageClientProvider::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
        delete_blob_context) noexcept {
  // Not implemented
  return absl::OkStatus();
}

absl::StatusOr<std::unique_ptr<BlobStorageClientProviderInterface>>
BlobStorageClientProviderFactory::Create(
    BlobStorageClientOptions options,
    InstanceClientProviderInterface* instance_client,
    core::AsyncExecutorInterface* cpu_async_executor,
    core::AsyncExecutorInterface* io_async_executor) noexcept {
  return std::make_unique<AzureBlobStorageClientProvider>(
      std::move(options), instance_client, cpu_async_executor,
      io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
