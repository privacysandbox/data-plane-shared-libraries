// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "scp/cc/public/cpio/validator/blob_storage_client_validator.h"

#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "scp/cc/core/interface/async_context.h"
#include "scp/cc/public/core/interface/errors.h"
#include "scp/cc/public/core/interface/execution_result.h"
#include "scp/cc/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "scp/cc/public/cpio/proto/blob_storage_service/v1/blob_storage_service.pb.h"
#include "scp/cc/public/cpio/validator/proto/validator_config.pb.h"

namespace google::scp::cpio::validator {

namespace {
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::scp::core::AsyncContext;
using google::scp::cpio::validator::proto::BlobStorageClientConfig;
using google::scp::cpio::validator::proto::GetBlobConfig;
using google::scp::cpio::validator::proto::ListBlobsMetadataConfig;
}  // namespace

void BlobStorageClientValidator::Run(
    const BlobStorageClientConfig& blob_storage_client_config) {
  auto blob_storage_client =
      google::scp::cpio::BlobStorageClientFactory::Create();
  google::scp::core::ExecutionResult result = blob_storage_client->Init();
  if (!result.Successful()) {
    std::cout << "FAILURE. Failed to Init BlobStorageClient. "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
    return;
  }
  result = blob_storage_client->Run();
  if (!result.Successful()) {
    std::cout << "FAILURE. Failed to Run BlobStorageClient. "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
    return;
  }
  for (const auto& get_blob_config_val :
       blob_storage_client_config.get_blob_config()) {
    RunGetBlobValidator(*blob_storage_client, get_blob_config_val);
  }
  for (const auto& list_blobs_metadata_config_val :
       blob_storage_client_config.list_blobs_metadata_config()) {
    RunListBlobsMetadataValidator(*blob_storage_client,
                                  list_blobs_metadata_config_val);
  }
}

void BlobStorageClientValidator::RunListBlobsMetadataValidator(
    google::scp::cpio::BlobStorageClientInterface& blob_storage_client,
    const ListBlobsMetadataConfig& list_blobs_metadata_config) {
  if (list_blobs_metadata_config.bucket_name().empty()) {
    std::cout << "FAILURE. ListBlobsMetadata failed. No bucket name provided."
              << std::endl;
    return;
  }
  // ListBlobsMetadata.
  absl::Notification finished;
  google::scp::core::ExecutionResult result;
  auto list_blobs_metadata_request =
      std::make_shared<ListBlobsMetadataRequest>();
  list_blobs_metadata_request->mutable_blob_metadata()->set_bucket_name(
      list_blobs_metadata_config.bucket_name());
  AsyncContext<ListBlobsMetadataRequest, ListBlobsMetadataResponse>
      list_blobs_metadata_context(
          std::move(list_blobs_metadata_request),
          [&result, &finished, &list_blobs_metadata_config](auto& context) {
            result = context.result;
            if (result.Successful()) {
              std::cout << "SUCCESS. ListBlobsMetadata succeeded. Bucket: "
                        << list_blobs_metadata_config.bucket_name()
                        << std::endl;
              LOG(INFO) << context.response->DebugString() << std::endl;
            }
            finished.Notify();
          });
  if (auto list_blobs_metadata_result =
          blob_storage_client.ListBlobsMetadata(list_blobs_metadata_context);
      !list_blobs_metadata_result.Successful()) {
    std::cout << "FAILURE. ListBlobsMetadata failed. Bucket: "
              << list_blobs_metadata_config.bucket_name() << " "
              << core::errors::GetErrorMessage(
                     list_blobs_metadata_result.status_code)
              << std::endl;
    return;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "FAILURE. ListBlobsMetadata failed. Bucket: "
              << list_blobs_metadata_config.bucket_name() << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
    return;
  }
}

void BlobStorageClientValidator::RunGetBlobValidator(
    google::scp::cpio::BlobStorageClientInterface& blob_storage_client,
    const GetBlobConfig& get_blob_config) {
  if (get_blob_config.bucket_name().empty()) {
    std::cout << "FAILURE. GetBlob failed. No bucket_name provided."
              << std::endl;
    return;
  }
  if (get_blob_config.blob_name().empty()) {
    std::cout << "FAILURE. GetBlob failed. No blob_name provided. Bucket: "
              << get_blob_config.bucket_name() << std::endl;
    return;
  }
  absl::Notification finished;
  google::scp::core::ExecutionResult result;
  auto get_blob_request = std::make_shared<GetBlobRequest>();
  auto* metadata = get_blob_request->mutable_blob_metadata();
  metadata->set_bucket_name(get_blob_config.bucket_name());
  metadata->set_blob_name(get_blob_config.blob_name());
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      std::move(get_blob_request),
      [&result, &finished, &get_blob_config](auto& context) {
        result = context.result;
        if (result.Successful()) {
          std::cout << "SUCCESS. GetBlob succeeded. Bucket: "
                    << get_blob_config.bucket_name()
                    << " Blob: " << get_blob_config.blob_name() << std::endl;
          LOG(INFO) << context.response->DebugString();
        }
        finished.Notify();
      });

  if (auto get_blob_result = blob_storage_client.GetBlob(get_blob_context);
      !get_blob_result.Successful()) {
    std::cout << "FAILURE. GetBlob failed. Bucket: "
              << get_blob_config.bucket_name()
              << " Blob: " << get_blob_config.blob_name() << " "
              << core::errors::GetErrorMessage(get_blob_result.status_code)
              << std::endl;
    return;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "FAILURE. GetBlob failed. Bucket: "
              << get_blob_config.bucket_name()
              << " Blob: " << get_blob_config.blob_name() << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
    return;
  }
}
}  // namespace google::scp::cpio::validator
