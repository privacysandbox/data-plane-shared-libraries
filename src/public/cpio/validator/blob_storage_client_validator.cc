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

#include "src/public/cpio/validator/blob_storage_client_validator.h"

#include <memory>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/core/interface/async_context.h"
#include "src/public/core/interface/errors.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/validator/proto/validator_config.pb.h"

namespace google::scp::cpio::validator {

namespace {
using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::scp::core::AsyncContext;
using google::scp::cpio::validator::proto::GetBlobConfig;
using google::scp::cpio::validator::proto::ListBlobsMetadataConfig;
}  // namespace

void RunListBlobsMetadataValidator(
    std::string_view name,
    const ListBlobsMetadataConfig& list_blobs_metadata_config) {
  if (list_blobs_metadata_config.bucket_name().empty()) {
    std::cout << "[ FAILURE ]  " << name << " No bucket name provided."
              << std::endl;
    return;
  }
  auto blob_storage_client =
      google::scp::cpio::BlobStorageClientFactory::Create();
  if (absl::Status error = blob_storage_client->Init(); !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
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
          [&result, &finished, &name](auto& context) {
            result = context.result;
            if (result.Successful()) {
              std::cout << "[ SUCCESS ] " << name << " " << std::endl;
              LOG(INFO) << context.response->DebugString() << std::endl;
            }
            finished.Notify();
          });
  if (absl::Status error =
          blob_storage_client->ListBlobsMetadata(list_blobs_metadata_context);
      !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
  }
}

void RunGetBlobValidator(std::string_view name,
                         const GetBlobConfig& get_blob_config) {
  if (get_blob_config.bucket_name().empty()) {
    std::cout << "[ FAILURE ] " << name << " No bucket_name provided."
              << std::endl;
    return;
  }
  if (get_blob_config.blob_name().empty()) {
    std::cout << "[ FAILURE ] " << name << " No blob_name provided."
              << std::endl;
    return;
  }
  auto blob_storage_client =
      google::scp::cpio::BlobStorageClientFactory::Create();
  if (absl::Status error = blob_storage_client->Init(); !error.ok()) {
    std::cout << "[ FAILURE ] " << name << " " << error << std::endl;
    return;
  }
  absl::Notification finished;
  google::scp::core::ExecutionResult result;
  auto get_blob_request = std::make_shared<GetBlobRequest>();
  auto* metadata = get_blob_request->mutable_blob_metadata();
  metadata->set_bucket_name(get_blob_config.bucket_name());
  metadata->set_blob_name(get_blob_config.blob_name());
  AsyncContext<GetBlobRequest, GetBlobResponse> get_blob_context(
      std::move(get_blob_request), [&result, &finished, &name](auto& context) {
        result = context.result;
        if (result.Successful()) {
          std::cout << "[ SUCCESS ] " << name << " " << std::endl;
          LOG(INFO) << context.response->DebugString();
        }
        finished.Notify();
      });

  if (absl::Status error = blob_storage_client->GetBlob(get_blob_context);
      !error.ok()) {
    std::cout << "[ FAILURE ]  " << name << " " << error << std::endl;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "[ FAILURE ] " << name << " "
              << core::errors::GetErrorMessage(result.status_code) << std::endl;
  }
}
}  // namespace google::scp::cpio::validator
