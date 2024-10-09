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

#include "gcp_cloud_storage.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "google/cloud/options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/object_read_stream.h"
#include "src/core/blob_storage_provider/common/error_codes.h"
#include "src/core/blob_storage_provider/gcp/gcp_cloud_storage_utils.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/blob_storage_provider_interface.h"
#include "src/core/interface/configuration_keys.h"
#include "src/core/interface/type_def.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/hashing.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::blob_storage_provider {
namespace {
using google::cloud::Options;
using google::cloud::StatusCode;
using google::cloud::storage::Client;
using google::cloud::storage::ComputeMD5Hash;
using google::cloud::storage::ConnectionPoolSizeOption;
using google::cloud::storage::DisableCrc32cChecksum;
using google::cloud::storage::EnableMD5Hash;
using google::cloud::storage::IdempotencyPolicyOption;
using google::cloud::storage::LimitedErrorCountRetryPolicy;
using google::cloud::storage::ListObjectsReader;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::ObjectReadStream;
using google::cloud::storage::Prefix;
using google::cloud::storage::ProjectIdOption;
using google::cloud::storage::RetryPolicyOption;
using google::cloud::storage::StartOffset;
using google::cloud::storage::StrictIdempotencyPolicy;
using google::scp::core::Blob;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::FinishContext;
using google::scp::core::GetBlobRequest;
using google::scp::core::GetBlobResponse;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::blob_storage_provider::GcpCloudStorageUtils;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB;
using google::scp::core::errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS;
using google::scp::core::utils::Base64Encode;

constexpr std::string_view kGcpCloudStorageProvider = "GcpCloudStorageProvider";
// TODO: Find ideal max concurrent connections and retry limit for operations
constexpr size_t kMaxConcurrentConnections = 1000;
constexpr size_t kRetryLimit = 3;
constexpr size_t kListBlobsMaxResults = 1000;

bool IsMarkerObject(const std::shared_ptr<std::string>& marker,
                    const ObjectMetadata& obj_metadata) {
  return marker && *marker == obj_metadata.name();
}

}  // namespace

ExecutionResult GcpCloudStorageProvider::CreateClientConfig() noexcept {
  // TODO: Look into other Options. See the following link below for accepted
  // inputs of Options. Additionally, Options is typically shared across GCP
  // services so there might be a better place to initialize and store this.
  // Note: Options can also be unset which may be useful for configuring things
  // like retry policies for specific executions.
  // https://googleapis.dev/cpp/google-cloud-common/2.2.1/classgoogle_1_1cloud_1_1Options.html
  client_config_ = std::make_shared<Options>();
  std::string project;
  auto execution_result =
      config_provider_->Get(std::string(kGcpProjectId), project);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  client_config_->set<ProjectIdOption>(project);
  client_config_->set<ConnectionPoolSizeOption>(kMaxConcurrentConnections);
  client_config_->set<RetryPolicyOption>(
      LimitedErrorCountRetryPolicy(kRetryLimit).clone());
  client_config_->set<IdempotencyPolicyOption>(
      StrictIdempotencyPolicy().clone());
  return SuccessExecutionResult();
}

void GcpCloudStorageProvider::CreateCloudStorage() noexcept {
  cloud_storage_client_shared_ = std::make_shared<Client>(*client_config_);
}

ExecutionResult GcpCloudStorageProvider::Init() noexcept {
  auto execution_result = CreateClientConfig();
  if (!execution_result.Successful()) {
    return execution_result;
  }
  CreateCloudStorage();
  return SuccessExecutionResult();
}

ExecutionResult GcpCloudStorageProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpCloudStorageProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpCloudStorageProvider::CreateBlobStorageClient(
    std::shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept {
  blob_storage_client = std::make_shared<GcpCloudStorageClient>(
      cloud_storage_client_shared_, async_executor_, io_async_executor_,
      async_execution_priority_, io_async_execution_priority_);
  return SuccessExecutionResult();
}

ExecutionResult GcpCloudStorageClient::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  const auto& request = *get_blob_context.request;
  if (!request.bucket_name || !request.blob_name ||
      request.bucket_name->empty() || request.blob_name->empty()) {
    return FailureExecutionResult(
        errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, get_blob_context] { GetBlobAsync(get_blob_context); },
          io_async_execution_priority_);
      !schedule_result.Successful()) {
    return schedule_result;
  }
  return SuccessExecutionResult();
}

void GcpCloudStorageClient::GetBlobAsync(
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  ObjectReadStream blob_stream = cloud_storage_client.ReadObject(
      *get_blob_context.request->bucket_name,
      *get_blob_context.request->blob_name, DisableCrc32cChecksum(true),
      EnableMD5Hash());
  if (!blob_stream || blob_stream.bad()) {
    SCP_DEBUG_CONTEXT(
        kGcpCloudStorageProvider, get_blob_context,
        "GcpCloudStorageProvider get blob request failed. Error code: %d, "
        "message: %s",
        blob_stream.status().code(), blob_stream.status().message().c_str());
    auto execution_result =
        GcpCloudStorageUtils::ConvertCloudStorageErrorToExecutionResult(
            blob_stream.status().code());
    FinishContext(execution_result, get_blob_context, async_executor_,
                  async_execution_priority_);
    return;
  }

  if (!blob_stream.size()) {
    SCP_DEBUG_CONTEXT(
        kGcpCloudStorageProvider, get_blob_context,
        "GcpCloudStorageProvider get blob request failed. Message: "
        "size missing.");
    FinishContext(FailureExecutionResult(
                      errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB),
                  get_blob_context, async_executor_, async_execution_priority_);
    return;
  }

  size_t content_length = *blob_stream.size();

  auto byte_buffer = std::make_shared<BytesBuffer>();
  byte_buffer->bytes = std::make_shared<std::vector<Byte>>(content_length);
  byte_buffer->length = content_length;
  byte_buffer->capacity = content_length;
  get_blob_context.response = std::make_shared<GetBlobResponse>();
  get_blob_context.response->buffer = std::move(byte_buffer);

  blob_stream.read(get_blob_context.response->buffer->bytes->data(),
                   content_length);
  ExecutionResult result = SuccessExecutionResult();
  if (blob_stream.bad()) {
    result =
        RetryExecutionResult(errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR);
    SCP_ERROR_CONTEXT(
        kGcpCloudStorageProvider, get_blob_context, result,
        "get blob request failed. Message: I/O error while reading "
        "blob stream.");
  } else if (blob_stream.fail()) {
    result =
        RetryExecutionResult(errors::SC_BLOB_STORAGE_PROVIDER_RETRIABLE_ERROR);
    SCP_ERROR_CONTEXT(
        kGcpCloudStorageProvider, get_blob_context, result,
        "get blob request failed. Message: Bad data encountered.");
  }

  FinishContext(result, get_blob_context, async_executor_,
                async_execution_priority_);
}

ExecutionResult GcpCloudStorageClient::ListBlobs(
    AsyncContext<ListBlobsRequest, ListBlobsResponse>&
        list_blobs_context) noexcept {
  const auto& request = *list_blobs_context.request;
  if (!request.bucket_name || request.bucket_name->empty()) {
    return FailureExecutionResult(
        errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, list_blobs_context] { ListBlobAsync(list_blobs_context); },
          io_async_execution_priority_);
      !schedule_result.Successful()) {
    return schedule_result;
  }
  return SuccessExecutionResult();
}

void GcpCloudStorageClient::ListBlobAsync(
    AsyncContext<ListBlobsRequest, ListBlobsResponse>
        list_blobs_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  auto objects_reader = [&list_blobs_context, &cloud_storage_client]() {
    auto prefix = list_blobs_context.request->blob_name == nullptr
                      ? Prefix()
                      : Prefix(*list_blobs_context.request->blob_name);
    auto max_results = MaxResults(kListBlobsMaxResults);
    if (list_blobs_context.request->marker == nullptr ||
        list_blobs_context.request->marker->empty()) {
      return cloud_storage_client.ListObjects(
          *list_blobs_context.request->bucket_name, prefix, max_results);
    } else {
      return cloud_storage_client.ListObjects(
          *list_blobs_context.request->bucket_name, prefix,
          StartOffset(*list_blobs_context.request->marker), max_results);
    }
  }();
  list_blobs_context.response = std::make_shared<ListBlobsResponse>();
  list_blobs_context.response->blobs = std::make_shared<std::vector<Blob>>();
  list_blobs_context.response->next_marker = nullptr;

  // GCP pagination happens through the iterator. All results are returned.
  for (auto&& object_metadata : objects_reader) {
    if (!object_metadata) {
      SCP_DEBUG_CONTEXT(
          kGcpCloudStorageProvider, list_blobs_context,
          absl::StrCat("GcpCloudStorageProvider list blobs request failed. "
                       "Error code: ",
                       object_metadata.status().code(),
                       "message: ", object_metadata.status().message()));
      auto execution_result =
          GcpCloudStorageUtils::ConvertCloudStorageErrorToExecutionResult(
              object_metadata.status().code());
      FinishContext(execution_result, list_blobs_context, async_executor_,
                    async_execution_priority_);
      return;
    }
    // If the first item returned is the same as the marker provided to this
    // call, then skip this object. This is because it was already included in
    // a previous call.
    if (list_blobs_context.response->blobs->empty() &&
        IsMarkerObject(list_blobs_context.request->marker, *object_metadata)) {
      continue;
    }
    Blob blob;
    blob.blob_name = std::make_shared<std::string>(object_metadata->name());
    blob.bucket_name = list_blobs_context.request->bucket_name;
    list_blobs_context.response->blobs->push_back(blob);
    if (list_blobs_context.response->blobs->size() == kListBlobsMaxResults) {
      // Force the page to end here, mark the final result in this page as the
      // "next" one to start at.
      // NOTE: There is an edge case where this query returns exactly
      // kListBlobsMaxResults in which case a next_marker is returned, but
      // calling ListBlobs again with this next_marker will actually return 0
      // results but the caller issued 2 RPCs. As this is an unlikely edge case,
      // we implement the https://en.wikipedia.org/wiki/Ostrich_algorithm
      list_blobs_context.response->next_marker = std::make_shared<Blob>(blob);
      break;
    }
  }
  FinishContext(SuccessExecutionResult(), list_blobs_context, async_executor_,
                async_execution_priority_);
}

ExecutionResult GcpCloudStorageClient::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) noexcept {
  const auto& request = *put_blob_context.request;
  if (!request.bucket_name || !request.blob_name ||
      request.bucket_name->empty() || request.blob_name->empty() ||
      request.buffer == nullptr) {
    return FailureExecutionResult(
        errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, put_blob_context] { PutBlobAsync(put_blob_context); },
          io_async_execution_priority_);
      !schedule_result.Successful()) {
    return schedule_result;
  }
  return SuccessExecutionResult();
}

void GcpCloudStorageClient::PutBlobAsync(
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);

  auto upload_obj = put_blob_context.request->buffer->ToString();
  std::string md5_hash = ComputeMD5Hash(upload_obj);
  auto object_metadata = cloud_storage_client.InsertObject(
      *put_blob_context.request->bucket_name,
      *put_blob_context.request->blob_name, std::move(upload_obj),
      MD5HashValue(md5_hash));
  if (!object_metadata) {
    SCP_DEBUG_CONTEXT(
        kGcpCloudStorageProvider, put_blob_context,
        "GcpCloudStorageProvider put blob request failed. Error code: %d, "
        "message: %s",
        object_metadata.status().code(),
        object_metadata.status().message().c_str());
    auto execution_result =
        GcpCloudStorageUtils::ConvertCloudStorageErrorToExecutionResult(
            object_metadata.status().code());
    FinishContext(execution_result, put_blob_context, async_executor_,
                  async_execution_priority_);
    return;
  }
  FinishContext(SuccessExecutionResult(), put_blob_context, async_executor_,
                async_execution_priority_);
}

ExecutionResult GcpCloudStorageClient::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
        delete_blob_context) noexcept {
  const auto& request = *delete_blob_context.request;
  if (!request.bucket_name || !request.blob_name ||
      request.bucket_name->empty() || request.blob_name->empty()) {
    return FailureExecutionResult(
        errors::SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, delete_blob_context] { DeleteBlobAsync(delete_blob_context); },
          io_async_execution_priority_);
      !schedule_result.Successful()) {
    return schedule_result;
  }
  return SuccessExecutionResult();
}

void GcpCloudStorageClient::DeleteBlobAsync(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>
        delete_blob_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  auto status = cloud_storage_client.DeleteObject(
      *delete_blob_context.request->bucket_name,
      *delete_blob_context.request->blob_name);
  if (!status.ok()) {
    SCP_DEBUG_CONTEXT(
        kGcpCloudStorageProvider, delete_blob_context,
        "GcpCloudStorageProvider delete blob request failed. Error code: %d, "
        "message: %s",
        status.code(), status.message().c_str());
    auto execution_result =
        GcpCloudStorageUtils::ConvertCloudStorageErrorToExecutionResult(
            status.code());
    FinishContext(execution_result, delete_blob_context, async_executor_,
                  async_execution_priority_);
    return;
  }
  FinishContext(SuccessExecutionResult(), delete_blob_context, async_executor_,
                async_execution_priority_);
}
}  // namespace google::scp::core::blob_storage_provider
