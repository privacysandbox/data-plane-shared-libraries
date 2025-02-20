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

#include "gcp_blob_storage_client_provider.h"

#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/util/time_util.h>

#include "absl/base/nullability.h"
#include "google/cloud/options.h"
#include "google/cloud/storage/client.h"
#include "google/cloud/storage/object_read_stream.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/interface/async_context.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/blob_storage_provider_interface.h"
#include "src/core/interface/configuration_keys.h"
#include "src/core/interface/type_def.h"
#include "src/core/utils/base64.h"
#include "src/core/utils/hashing.h"
#include "src/cpio/client_providers/blob_storage_client_provider/common/error_codes.h"
#include "src/cpio/client_providers/instance_client_provider/gcp/gcp_instance_client_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"
#include "src/util/status_macro/status_macros.h"

#include "gcp_blob_storage_client_utils.h"

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
using google::cloud::storage::MatchGlob;
using google::cloud::storage::MaxResults;
using google::cloud::storage::MD5HashValue;
using google::cloud::storage::NewResumableUploadSession;
using google::cloud::storage::ObjectMetadata;
using google::cloud::storage::ObjectReadStream;
using google::cloud::storage::Prefix;
using google::cloud::storage::ProjectIdOption;
using google::cloud::storage::ReadRange;
using google::cloud::storage::RestoreResumableUploadSession;
using google::cloud::storage::RetryPolicyOption;
using google::cloud::storage::StartOffset;
using google::cloud::storage::StrictIdempotencyPolicy;
using google::cloud::storage::TransferStallTimeoutOption;
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
using google::scp::cpio::client_providers::GcpInstanceClientUtils;

namespace {

constexpr size_t kMaxConcurrentConnections = 1000;
constexpr size_t kListBlobsMetadataMaxResults = 1000;
constexpr size_t k64KbCount = 64 << 10;
constexpr std::chrono::nanoseconds kDefaultStreamKeepaliveNanos =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::minutes(5));
constexpr std::chrono::nanoseconds kMaximumStreamKeepaliveNanos =
    std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::minutes(10));
constexpr std::chrono::seconds kPutBlobRescanTime = std::chrono::seconds(5);
constexpr char kExcludeDirectoriesMatchGlob[] = "**[^/]";

bool IsPageTokenObject(const ListBlobsMetadataRequest& list_blobs_request,
                       const ObjectMetadata& obj_metadata) {
  return list_blobs_request.has_page_token() &&
         list_blobs_request.page_token() == obj_metadata.name();
}

uint64_t GetMaxPageSize(const ListBlobsMetadataRequest& list_blobs_request) {
  return list_blobs_request.has_max_page_size()
             ? list_blobs_request.max_page_size()
             : kListBlobsMetadataMaxResults;
}

}  // namespace

namespace google::scp::cpio::client_providers {

absl::Status GcpBlobStorageClientProvider::Init() noexcept {
  // Try to get project_id from options, otherwise get project_id from running
  // instance_client.
  BlobStorageClientOptions options = options_;

  if (options.project_id.empty()) {
    auto project_id_or =
        GcpInstanceClientUtils::GetCurrentProjectId(*instance_client_);
    if (!project_id_or.Successful()) {
      SCP_ERROR(kGcpBlobStorageClientProvider, kZeroUuid,
                project_id_or.result(),
                "Failed to get project ID for current instance");
      return absl::InternalError(google::scp::core::errors::GetErrorMessage(
          project_id_or.result().status_code));
    }
    options.project_id = std::move(*project_id_or);
  }

  auto client_or = cloud_storage_factory_->CreateClient(std::move(options));
  if (!client_or.Successful()) {
    SCP_ERROR(kGcpBlobStorageClientProvider, kZeroUuid, client_or.result(),
              "Failed creating Google Cloud Storage client.");
    return absl::UnknownError(google::scp::core::errors::GetErrorMessage(
        client_or.result().status_code));
  }
  cloud_storage_client_shared_ = std::move(*client_or);
  return absl::OkStatus();
}

absl::Status GcpBlobStorageClientProvider::GetBlob(
    AsyncContext<GetBlobRequest, GetBlobResponse>& get_blob_context) noexcept {
  const auto& request = *get_blob_context.request;
  if (request.blob_metadata().bucket_name().empty() ||
      request.blob_metadata().blob_name().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_context,
                      execution_result,
                      "Get blob request is missing bucket or blob name");
    get_blob_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }
  if (request.has_byte_range() && request.byte_range().begin_byte_index() >
                                      request.byte_range().end_byte_index()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_context, execution_result,
        "Get blob request provides begin_byte_index that is larger "
        "than end_byte_index");
    get_blob_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, get_blob_context] { GetBlobInternal(get_blob_context); },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_context,
                      schedule_result,
                      "Get blob request failed to be scheduled");
    get_blob_context.Finish(schedule_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }
  return absl::OkStatus();
}

void GcpBlobStorageClientProvider::GetBlobInternal(
    AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);

  ReadRange read_range;
  if (get_blob_context.request->has_byte_range()) {
    // ReadRange is right-open and ByteRange::end_byte_index is said to be
    // inclusive, add one.
    read_range =
        ReadRange(get_blob_context.request->byte_range().begin_byte_index(),
                  get_blob_context.request->byte_range().end_byte_index() + 1);
  }
  ObjectReadStream blob_stream = cloud_storage_client.ReadObject(
      get_blob_context.request->blob_metadata().bucket_name(),
      get_blob_context.request->blob_metadata().blob_name(),
      DisableCrc32cChecksum(true), EnableMD5Hash(), read_range);
  // ValidateStream checks if the blob_stream status is successful. If not, it
  // will invoke FinishContext and return an error. In the empty blob case,
  // the blob_stream should still be successful.
  if (!ValidateStream(get_blob_context, blob_stream).Successful()) {
    return;
  }

  get_blob_context.response = std::make_shared<GetBlobResponse>();
  get_blob_context.response->mutable_blob()->mutable_metadata()->CopyFrom(
      get_blob_context.request->blob_metadata());

  // blob_stream.size() always has the full size of the object, including when
  // byte ranges are requested. The blob size isn't set (std::optional) if the
  // requested blob size is zero.
  if (!blob_stream.size().has_value()) {
    FinishContext(SuccessExecutionResult(), get_blob_context,
                  *cpu_async_executor_);
    return;
  }

  size_t content_length = *blob_stream.size();
  if (get_blob_context.request->has_byte_range()) {
    const size_t max_end_index = content_length - 1;
    // If the end byte is beyond the size of the object, truncate to the end of
    // the object.
    size_t end_index =
        get_blob_context.request->byte_range().end_byte_index() > max_end_index
            ? max_end_index
            : get_blob_context.request->byte_range().end_byte_index();
    content_length = 1 + end_index -
                     get_blob_context.request->byte_range().begin_byte_index();
  }
  auto& blob_bytes = *get_blob_context.response->mutable_blob()->mutable_data();
  blob_bytes.resize(content_length);
  blob_stream.read(blob_bytes.data(), content_length);
  if (!ValidateStream(get_blob_context, blob_stream).Successful()) {
    return;
  }
  FinishContext(SuccessExecutionResult(), get_blob_context,
                *cpu_async_executor_);
}

absl::Status GcpBlobStorageClientProvider::GetBlobStream(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>&
        get_blob_stream_context) noexcept {
  const auto& request = *get_blob_stream_context.request;
  if (request.blob_metadata().bucket_name().empty() ||
      request.blob_metadata().blob_name().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      execution_result,
                      "Get blob stream request is missing bucket or blob name");
    get_blob_stream_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }
  if (request.has_byte_range() && request.byte_range().begin_byte_index() >
                                      request.byte_range().end_byte_index()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_stream_context,
        execution_result,
        "Get blob stream request provides begin_byte_index that is larger "
        "than end_byte_index");
    get_blob_stream_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, get_blob_stream_context] {
            GetBlobStreamInternal(get_blob_stream_context, nullptr /*tracker*/);
          },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      schedule_result,
                      "Get blob stream request failed to be scheduled");
    get_blob_stream_context.Finish(schedule_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }
  return absl::OkStatus();
}

void GcpBlobStorageClientProvider::GetBlobStreamInternal(
    ConsumerStreamingContext<GetBlobStreamRequest, GetBlobStreamResponse>
        get_blob_stream_context,
    std::shared_ptr<GetBlobStreamTracker> tracker) noexcept {
  if (!tracker) {
    if (auto tracker_or = InitGetBlobStreamTracker(get_blob_stream_context);
        tracker_or.Successful()) {
      tracker = std::move(*tracker_or);
    } else {
      return;
    }
  }
  if (get_blob_stream_context.IsCancelled()) {
    auto result = FailureExecutionResult(
        SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      result, "Get blob stream request was cancelled.");
    FinishStreamingContext(result, get_blob_stream_context,
                           *cpu_async_executor_);
    return;
  }
  auto response = ReadNextPortion(*get_blob_stream_context.request, *tracker);

  if (!ValidateStream(get_blob_stream_context, tracker->stream).Successful()) {
    return;
  }

  auto push_result =
      get_blob_stream_context.TryPushResponse(std::move(response));
  if (!push_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      push_result, "Failed to push new message.");
    FinishStreamingContext(push_result, get_blob_stream_context,
                           *cpu_async_executor_);
    return;
  }

  // Schedule processing the next message.
  auto schedule_result = cpu_async_executor_->Schedule(
      [get_blob_stream_context]() mutable {
        get_blob_stream_context.ProcessNextMessage();
      },
      AsyncPriority::Normal);
  if (!schedule_result.Successful()) {
    get_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, get_blob_stream_context,
        get_blob_stream_context.result,
        "Get blob stream process next message failed to be scheduled");
    FinishStreamingContext(schedule_result, get_blob_stream_context,
                           *cpu_async_executor_);
  }

  if (tracker->bytes_remaining == 0) {
    FinishStreamingContext(SuccessExecutionResult(), get_blob_stream_context,
                           *cpu_async_executor_);
    return;
  }

  // Schedule reading the next section.
  schedule_result = io_async_executor_->Schedule(
      [this, get_blob_stream_context, tracker = std::move(tracker)] {
        GetBlobStreamInternal(get_blob_stream_context, std::move(tracker));
      },
      AsyncPriority::Normal);
  if (!schedule_result.Successful()) {
    get_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, get_blob_stream_context,
                      get_blob_stream_context.result,
                      "Get blob stream follow up read failed to be scheduled");
    FinishStreamingContext(schedule_result, get_blob_stream_context,
                           *cpu_async_executor_);
  }
}

ExecutionResultOr<
    std::shared_ptr<GcpBlobStorageClientProvider::GetBlobStreamTracker>>
GcpBlobStorageClientProvider::InitGetBlobStreamTracker(
    core::ConsumerStreamingContext<
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
        cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
        context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  // Set up the tracker to the beginning.
  auto tracker = std::make_shared<GetBlobStreamTracker>();
  ReadRange read_range;
  if (context.request->has_byte_range()) {
    // ReadRange is right-open and ByteRange::end_byte_index is said to be
    // inclusive, add one.
    read_range = ReadRange(context.request->byte_range().begin_byte_index(),
                           context.request->byte_range().end_byte_index() + 1);
  }
  tracker->stream = cloud_storage_client.ReadObject(
      context.request->blob_metadata().bucket_name(),
      context.request->blob_metadata().blob_name(), DisableCrc32cChecksum(true),
      EnableMD5Hash(), read_range);
  auto& blob_stream = tracker->stream;
  auto validate_result = ValidateStream(context, blob_stream);
  if (!validate_result.Successful()) {
    return validate_result;
  }

  if (!blob_stream.size()) {
    auto result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, context, result,
                      "Get blob stream request failed. Message: size missing.");
    FinishStreamingContext(result, context, *cpu_async_executor_);
    return result;
  }

  // blob_stream.size() always has the full size of the object, not just the
  // read range.
  size_t content_length = *blob_stream.size();
  if (context.request->has_byte_range()) {
    const size_t max_end_index = content_length - 1;
    // If the end byte is beyond the size of the object, truncate to the end
    // of the object.
    size_t end_index =
        context.request->byte_range().end_byte_index() > max_end_index
            ? max_end_index
            : context.request->byte_range().end_byte_index();
    content_length =
        1 + end_index - context.request->byte_range().begin_byte_index();
  }
  tracker->bytes_remaining = content_length;
  // The first portion will start at begin_byte_index.
  tracker->last_end_byte_index =
      context.request->byte_range().begin_byte_index() - 1;
  return tracker;
}

GetBlobStreamResponse GcpBlobStorageClientProvider::ReadNextPortion(
    const GetBlobStreamRequest& request,
    GetBlobStreamTracker& tracker) noexcept {
  auto& blob_stream = tracker.stream;
  // If max_bytes_per_response is provided, use it. Otherwise use 64KB.
  size_t next_read_size = request.max_bytes_per_response() == 0
                              ? k64KbCount
                              : request.max_bytes_per_response();
  // Read up to next_read_size or bytes_remaining.
  next_read_size = std::min(next_read_size, tracker.bytes_remaining);

  GetBlobStreamResponse response;
  response.mutable_blob_portion()->mutable_metadata()->CopyFrom(
      request.blob_metadata());
  // We begin one past where we ended last.
  response.mutable_byte_range()->set_begin_byte_index(
      tracker.last_end_byte_index + 1);
  // We end one space before the read size.
  response.mutable_byte_range()->set_end_byte_index(
      response.byte_range().begin_byte_index() + next_read_size - 1);

  auto& blob_bytes = *response.mutable_blob_portion()->mutable_data();
  blob_bytes.resize(next_read_size);

  blob_stream.read(blob_bytes.data(), next_read_size);
  tracker.bytes_remaining -= next_read_size;
  tracker.last_end_byte_index = response.byte_range().end_byte_index();
  return response;
}

absl::Status GcpBlobStorageClientProvider::ListBlobsMetadata(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>&
        list_blobs_context) noexcept {
  const auto& request = *list_blobs_context.request;
  if (request.blob_metadata().bucket_name().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, list_blobs_context,
                      execution_result,
                      "List blobs metadata request failed. Bucket name empty.");
    list_blobs_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }
  if (request.has_max_page_size() && request.max_page_size() > 1000) {
    list_blobs_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, list_blobs_context,
        list_blobs_context.result,
        "List blobs metadata request failed. Max page size cannot be "
        "greater than 1000.");
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            list_blobs_context.result.status_code));
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, list_blobs_context] {
            ListBlobsMetadataInternal(list_blobs_context);
          },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, list_blobs_context,
                      schedule_result,
                      "List blobs metadata request failed to be scheduled");
    list_blobs_context.Finish(schedule_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }
  return absl::OkStatus();
}

void GcpBlobStorageClientProvider::ListBlobsMetadataInternal(
    AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
        list_blobs_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  const auto& request = *list_blobs_context.request;
  auto objects_reader = [&request, &cloud_storage_client]() {
    auto prefix = request.blob_metadata().blob_name().empty()
                      ? Prefix()
                      : Prefix(request.blob_metadata().blob_name());
    auto max_page_size = request.has_max_page_size()
                             ? request.max_page_size()
                             : kListBlobsMetadataMaxResults;
    auto match_glob = request.exclude_directories()
                          ? MatchGlob(kExcludeDirectoriesMatchGlob)
                          : MatchGlob();
    auto max_results = MaxResults(max_page_size);
    if (!request.has_page_token() || request.page_token().empty()) {
      return cloud_storage_client.ListObjects(
          request.blob_metadata().bucket_name(), prefix, max_results,
          match_glob);
    } else {
      return cloud_storage_client.ListObjects(
          request.blob_metadata().bucket_name(), prefix,
          StartOffset(request.page_token()), max_results, match_glob);
    }
  }();
  list_blobs_context.response = std::make_shared<ListBlobsMetadataResponse>();

  // GCP pagination happens through the iterator. All results are returned.
  for (auto&& object_metadata : objects_reader) {
    if (!object_metadata) {
      auto execution_result =
          GcpBlobStorageClientUtils::ConvertCloudStorageErrorToExecutionResult(
              object_metadata.status().code());
      SCP_ERROR_CONTEXT(
          kGcpBlobStorageClientProvider, list_blobs_context, execution_result,
          "List blobs request failed. Error code: %d, message: %s",
          object_metadata.status().code(),
          object_metadata.status().message().c_str());
      FinishContext(execution_result, list_blobs_context, *cpu_async_executor_);
      return;
    }
    // If the first item returned is the same as the marker provided to this
    // call, then skip this object. This is because it was already included in
    // a previous call.
    if (list_blobs_context.response->blob_metadatas().empty() &&
        IsPageTokenObject(request, *object_metadata)) {
      continue;
    }
    BlobMetadata blob_metadata;
    blob_metadata.set_blob_name(object_metadata->name());
    blob_metadata.set_bucket_name(request.blob_metadata().bucket_name());
    *list_blobs_context.response->add_blob_metadatas() =
        std::move(blob_metadata);
    if (list_blobs_context.response->blob_metadatas().size() ==
        GetMaxPageSize(request)) {
      // Force the page to end here, mark the final result in this page as
      // the "next" one to start at. NOTE: There is an edge case where this
      // query returns exactly GetMaxPageSize in which case a next_marker is
      // returned, but calling ListBlobs again with this next_marker will
      // actually return 0 results but the caller issued 2 RPCs. As this is
      // an unlikely edge case, we implement the
      // https://en.wikipedia.org/wiki/Ostrich_algorithm
      list_blobs_context.response->set_next_page_token(object_metadata->name());
      break;
    }
  }
  FinishContext(SuccessExecutionResult(), list_blobs_context,
                *cpu_async_executor_);
}

absl::Status GcpBlobStorageClientProvider::PutBlob(
    AsyncContext<PutBlobRequest, PutBlobResponse>& put_blob_context) noexcept {
  const auto& request = *put_blob_context.request;
  if (request.blob().metadata().bucket_name().empty() ||
      request.blob().metadata().blob_name().empty() ||
      request.blob().data().empty()) {
    put_blob_context.result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_context,
                      put_blob_context.result,
                      "Put blob request failed. Ensure that bucket name, blob "
                      "name, and data are present.");
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            put_blob_context.result.status_code));
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, put_blob_context] { PutBlobInternal(put_blob_context); },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_context,
                      schedule_result,
                      "Put blob request failed to be scheduled");
    put_blob_context.Finish(schedule_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }
  return absl::OkStatus();
}

void GcpBlobStorageClientProvider::PutBlobInternal(
    AsyncContext<PutBlobRequest, PutBlobResponse> put_blob_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);

  const auto& request = *put_blob_context.request;
  std::string md5_hash = ComputeMD5Hash(request.blob().data());
  auto object_metadata = cloud_storage_client.InsertObject(
      request.blob().metadata().bucket_name(),
      request.blob().metadata().blob_name(), request.blob().data(),
      MD5HashValue(md5_hash));
  if (!object_metadata) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_context,
                      put_blob_context.result,
                      "Put blob request failed. Error code: %d, message: %s",
                      object_metadata.status().code(),
                      object_metadata.status().message().c_str());
    auto execution_result =
        GcpBlobStorageClientUtils::ConvertCloudStorageErrorToExecutionResult(
            object_metadata.status().code());
    FinishContext(execution_result, put_blob_context, *cpu_async_executor_);
    return;
  }
  put_blob_context.response = std::make_shared<PutBlobResponse>();
  FinishContext(SuccessExecutionResult(), put_blob_context,
                *cpu_async_executor_);
}

absl::Status GcpBlobStorageClientProvider::PutBlobStream(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>&
        put_blob_stream_context) noexcept {
  const auto& request = *put_blob_stream_context.request;
  if (request.blob_portion().metadata().bucket_name().empty() ||
      request.blob_portion().metadata().blob_name().empty() ||
      request.blob_portion().data().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, put_blob_stream_context,
        execution_result,
        "Put blob stream request failed. Ensure that bucket name, blob "
        "name, and data are present.");
    put_blob_stream_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            put_blob_stream_context.result.status_code));
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, put_blob_stream_context] {
            InitPutBlobStream(put_blob_stream_context);
          },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      schedule_result,
                      "Put blob stream request failed to be scheduled");
    put_blob_stream_context.Finish(schedule_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }
  return absl::OkStatus();
}

void GcpBlobStorageClientProvider::InitPutBlobStream(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        put_blob_stream_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  const auto& request = *put_blob_stream_context.request;
  auto tracker = std::make_shared<PutBlobStreamTracker>();
  auto duration =
      request.has_stream_keepalive_duration()
          ? std::chrono::nanoseconds(TimeUtil::DurationToNanoseconds(
                request.stream_keepalive_duration()))
          : kDefaultStreamKeepaliveNanos;
  if (duration > kMaximumStreamKeepaliveNanos) {
    auto result = FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, put_blob_stream_context, result,
        "Supplied keepalive duration is greater than the maximum of "
        "10 minutes.");
    FinishStreamingContext(result, put_blob_stream_context,
                           *cpu_async_executor_);
    return;
  }
  tracker->expiry_time_ns =
      TimeProvider::GetWallTimestampInNanoseconds() + duration;

  tracker->bucket_name = request.blob_portion().metadata().bucket_name();
  tracker->blob_name = request.blob_portion().metadata().blob_name();
  tracker->stream = cloud_storage_client.WriteObject(
      tracker->bucket_name, tracker->blob_name, NewResumableUploadSession());
  // Write the initial data from the first request.
  tracker->stream.write(request.blob_portion().data().c_str(),
                        request.blob_portion().data().length());
  if (!ValidateStream(put_blob_stream_context, tracker->stream).Successful()) {
    return;
  }
  PutBlobStreamInternal(put_blob_stream_context, tracker);
}

void GcpBlobStorageClientProvider::RestoreUploadIfSuspended(
    PutBlobStreamTracker& tracker, Client& cloud_storage_client) noexcept {
  if (tracker.session_id.has_value()) {
    // We suspended the upload previously, pick it up here.
    tracker.stream = cloud_storage_client.WriteObject(
        tracker.bucket_name, tracker.blob_name,
        RestoreResumableUploadSession(*tracker.session_id));
    tracker.session_id.reset();
  }
}

void GcpBlobStorageClientProvider::PutBlobStreamInternal(
    ProducerStreamingContext<PutBlobStreamRequest, PutBlobStreamResponse>
        put_blob_stream_context,
    std::shared_ptr<PutBlobStreamTracker> tracker) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);

  if (put_blob_stream_context.IsCancelled()) {
    RestoreUploadIfSuspended(*tracker, cloud_storage_client);
    auto session_id = tracker->stream.resumable_session_id();
    // Cancel any outstanding uploads.
    std::move(tracker->stream).Suspend();
    cloud_storage_client.DeleteResumableUpload(session_id);
    auto result = FailureExecutionResult(
        SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_CANCELLED);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      result, "Put blob stream request was cancelled");
    FinishStreamingContext(result, put_blob_stream_context,
                           *cpu_async_executor_);
    return;
  }

  // If there's no message, schedule again. If there's a message - write it.
  auto request = put_blob_stream_context.TryGetNextRequest();
  if (request == nullptr) {
    if (put_blob_stream_context.IsMarkedDone()) {
      RestoreUploadIfSuspended(*tracker, cloud_storage_client);
      // We've processed all messages and there won't be any more.
      tracker->stream.Close();
      auto object_metadata = tracker->stream.metadata();
      auto result = SuccessExecutionResult();
      if (!object_metadata) {
        result = GcpBlobStorageClientUtils::
            ConvertCloudStorageErrorToExecutionResult(
                object_metadata.status().code());
        SCP_ERROR_CONTEXT(
            kGcpBlobStorageClientProvider, put_blob_stream_context, result,
            "Put blob stream request failed. Error code: %d, message: %s",
            object_metadata.status().code(),
            object_metadata.status().message().c_str());
      }
      put_blob_stream_context.response =
          std::make_shared<PutBlobStreamResponse>();
      FinishStreamingContext(result, put_blob_stream_context,
                             *cpu_async_executor_);
      return;
    }
    // If this session expired, cancel the upload and finish.
    if (TimeProvider::GetWallTimestampInNanoseconds() >=
        tracker->expiry_time_ns) {
      auto result = FailureExecutionResult(
          SC_BLOB_STORAGE_PROVIDER_STREAM_SESSION_EXPIRED);
      SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                        result, "Put blob stream session expired.");
      auto session_id =
          tracker->session_id.value_or(tracker->stream.resumable_session_id());
      // Cancel any outstanding uploads.
      std::move(tracker->stream).Suspend();
      cloud_storage_client.DeleteResumableUpload(session_id);
      FinishStreamingContext(result, put_blob_stream_context,
                             *cpu_async_executor_);
      return;
    }
    // No message is available but we're holding a session - let's suspend it.
    if (!tracker->session_id.has_value()) {
      tracker->session_id = tracker->stream.resumable_session_id();
      std::move(tracker->stream).Suspend();
    }
    // Schedule checking for a new message.
    auto schedule_result = io_async_executor_->ScheduleFor(
        [this, put_blob_stream_context, tracker] {
          PutBlobStreamInternal(put_blob_stream_context, tracker);
        },
        (TimeProvider::GetSteadyTimestampInNanoseconds() + kPutBlobRescanTime)
            .count());
    if (!schedule_result.Successful()) {
      put_blob_stream_context.result = schedule_result;
      SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                        put_blob_stream_context.result,
                        "Put blob stream request failed to be scheduled");
      FinishStreamingContext(schedule_result, put_blob_stream_context,
                             *cpu_async_executor_);
    }
    return;
  }
  // Validate that the new request specifies the same blob.
  if (request->blob_portion().metadata().bucket_name() !=
          tracker->bucket_name ||
      request->blob_portion().metadata().blob_name() != tracker->blob_name) {
    auto result = FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      result,
                      "Enqueued message does not specify the same blob (bucket "
                      "name, blob name) as previously.");
    FinishStreamingContext(result, put_blob_stream_context,
                           *cpu_async_executor_);
    return;
  }
  RestoreUploadIfSuspended(*tracker, cloud_storage_client);
  tracker->stream.write(request->blob_portion().data().c_str(),
                        request->blob_portion().data().length());
  if (!ValidateStream(put_blob_stream_context, tracker->stream).Successful()) {
    return;
  }
  // Schedule uploading the next portion.
  auto schedule_result = io_async_executor_->Schedule(
      [this, put_blob_stream_context, tracker] {
        PutBlobStreamInternal(put_blob_stream_context, tracker);
      },
      AsyncPriority::Normal);
  if (!schedule_result.Successful()) {
    put_blob_stream_context.result = schedule_result;
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, put_blob_stream_context,
                      put_blob_stream_context.result,
                      "Put blob stream request failed to be scheduled");
    FinishStreamingContext(schedule_result, put_blob_stream_context,
                           *cpu_async_executor_);
  }
}

absl::Status GcpBlobStorageClientProvider::DeleteBlob(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
        delete_blob_context) noexcept {
  const auto& request = *delete_blob_context.request;
  if (request.blob_metadata().bucket_name().empty() ||
      request.blob_metadata().blob_name().empty()) {
    auto execution_result =
        FailureExecutionResult(SC_BLOB_STORAGE_PROVIDER_INVALID_ARGS);
    SCP_ERROR_CONTEXT(
        kGcpBlobStorageClientProvider, delete_blob_context, execution_result,
        "Delete blob request failed. Missing bucket or blob name.");
    delete_blob_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  if (auto schedule_result = io_async_executor_->Schedule(
          [this, delete_blob_context] {
            DeleteBlobInternal(delete_blob_context);
          },
          AsyncPriority::Normal);
      !schedule_result.Successful()) {
    SCP_ERROR_CONTEXT(kGcpBlobStorageClientProvider, delete_blob_context,
                      schedule_result,
                      "Delete blob request failed to be scheduled");
    delete_blob_context.Finish(schedule_result);
    return absl::InternalError(google::scp::core::errors::GetErrorMessage(
        schedule_result.status_code));
  }
  return absl::OkStatus();
}

void GcpBlobStorageClientProvider::DeleteBlobInternal(
    AsyncContext<DeleteBlobRequest, DeleteBlobResponse>
        delete_blob_context) noexcept {
  Client cloud_storage_client(*cloud_storage_client_shared_);
  auto status = cloud_storage_client.DeleteObject(
      delete_blob_context.request->blob_metadata().bucket_name(),
      delete_blob_context.request->blob_metadata().blob_name());
  if (!status.ok()) {
    SCP_DEBUG_CONTEXT(kGcpBlobStorageClientProvider, delete_blob_context,
                      "Delete blob request failed. Error code: %d, message: %s",
                      status.code(), status.message().c_str());
    auto execution_result =
        GcpBlobStorageClientUtils::ConvertCloudStorageErrorToExecutionResult(
            status.code());
    FinishContext(execution_result, delete_blob_context, *cpu_async_executor_);
    return;
  }
  delete_blob_context.response = std::make_shared<DeleteBlobResponse>();
  FinishContext(SuccessExecutionResult(), delete_blob_context,
                *cpu_async_executor_);
}

Options GcpCloudStorageFactory::CreateClientOptions(
    BlobStorageClientOptions options) noexcept {
  Options client_options;
  client_options.set<ProjectIdOption>(std::move(options.project_id));
  client_options.set<ConnectionPoolSizeOption>(kMaxConcurrentConnections);
  client_options.set<RetryPolicyOption>(
      LimitedErrorCountRetryPolicy(options.retry_limit).clone());
  client_options.set<IdempotencyPolicyOption>(
      StrictIdempotencyPolicy().clone());
  client_options.set<TransferStallTimeoutOption>(
      options.transfer_stall_timeout);
  return client_options;
}

core::ExecutionResultOr<std::unique_ptr<Client>>
GcpCloudStorageFactory::CreateClient(
    BlobStorageClientOptions options) noexcept {
  return std::make_unique<Client>(CreateClientOptions(std::move(options)));
}

absl::StatusOr<std::unique_ptr<BlobStorageClientProviderInterface>>
BlobStorageClientProviderFactory::Create(
    BlobStorageClientOptions options,
    absl::Nonnull<InstanceClientProviderInterface*> instance_client,
    absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
    absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor) noexcept {
  auto provider = std::make_unique<GcpBlobStorageClientProvider>(
      std::move(options), instance_client, cpu_async_executor,
      io_async_executor);
  PS_RETURN_IF_ERROR(provider->Init());
  return provider;
}
}  // namespace google::scp::cpio::client_providers
