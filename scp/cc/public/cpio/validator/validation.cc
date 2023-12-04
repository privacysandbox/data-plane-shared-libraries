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

#include <fcntl.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <string_view>
#include <utility>

#include <google/protobuf/text_format.h>

#include "absl/flags/parse.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/synchronization/notification.h"
#include "core/common/operation_dispatcher/src/operation_dispatcher.h"
#include "cpio/client_providers/global_cpio/src/global_cpio.h"
#include "public/core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "public/cpio/interface/cpio.h"
#include "scp/cc/public/cpio/validator/proto/validator_config.pb.h"

using google::cmrt::sdk::blob_storage_service::v1::GetBlobRequest;
using google::cmrt::sdk::blob_storage_service::v1::GetBlobResponse;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest;
using google::cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse;
using google::scp::core::AsyncContext;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::validator::proto::BlobStorageConfig;
using google::scp::cpio::validator::proto::GetBlobConfig;
using google::scp::cpio::validator::proto::ListBlobsMetadataConfig;
using google::scp::cpio::validator::proto::ValidatorConfig;

namespace {
constexpr char kValidatorConfigPath[] = "/etc/validator_config.txtpb";
}  // namespace

std::string_view GetValidatorFailedToRunMsg() {
  return "FAILURE. Could not run all validation tests. For details, see above.";
}

void RunBlobStorageGetBlobValidation(
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
          std::cout << "SUCCESS. GetBlob succeed. Bucket: "
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
              << GetErrorMessage(get_blob_result.status_code) << std::endl;
    return;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "FAILURE. GetBlob failed. Bucket: "
              << get_blob_config.bucket_name()
              << " Blob: " << get_blob_config.blob_name() << " "
              << GetErrorMessage(result.status_code) << std::endl;
    return;
  }
}

void RunBlobStoragListBlobsMetadataValidation(
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
              << GetErrorMessage(list_blobs_metadata_result.status_code)
              << std::endl;
    return;
  }
  finished.WaitForNotification();
  if (!result.Successful()) {
    std::cout << "FAILURE. ListBlobsMetadata failed. Bucket: "
              << list_blobs_metadata_config.bucket_name() << " "
              << GetErrorMessage(result.status_code) << std::endl;
    return;
  }
}

void RunBlobStorageValidation(const BlobStorageConfig& blob_storage_config) {
  auto blob_storage_client =
      google::scp::cpio::BlobStorageClientFactory::Create();
  google::scp::core::ExecutionResult result = blob_storage_client->Init();
  if (!result.Successful()) {
    std::cout << "FAILURE. Failed to Init BlobStorageClient. "
              << GetErrorMessage(result.status_code) << std::endl;
    return;
  }
  result = blob_storage_client->Run();
  if (!result.Successful()) {
    std::cout << "FAILURE. Failed to Run BlobStorageClient. "
              << GetErrorMessage(result.status_code) << std::endl;
    return;
  }
  for (const auto& get_blob_config_val :
       blob_storage_config.get_blob_config()) {
    RunBlobStorageGetBlobValidation(*blob_storage_client, get_blob_config_val);
  }
  for (const auto& list_blobs_metadata_config_val :
       blob_storage_config.list_blobs_metadata_config()) {
    RunBlobStoragListBlobsMetadataValidation(*blob_storage_client,
                                             list_blobs_metadata_config_val);
  }
}

int main(int argc, char* argv[]) {
  // Process command line parameters
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  ValidatorConfig validator_config;
  int fd = open(kValidatorConfigPath, O_RDONLY);
  if (fd < 0) {
    std::cout << "FAILURE. Unable to open validator config file." << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::protobuf::io::FileInputStream file_input_stream(fd);
  file_input_stream.SetCloseOnDelete(true);

  if (!google::protobuf::TextFormat::Parse(&file_input_stream,
                                           &validator_config)) {
    std::cout << std::endl
              << "FAILURE. Unable to parse validator config file." << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  google::scp::cpio::CpioOptions cpio_options;
  cpio_options.log_option = google::scp::cpio::LogOption::kNoLog;

  if (google::scp::core::ExecutionResult result =
          google::scp::cpio::Cpio::InitCpio(cpio_options);
      !result.Successful()) {
    std::cout << "FAILURE. Unable to initialize CPIO: "
              << GetErrorMessage(result.status_code) << std::endl;
    std::cout << GetValidatorFailedToRunMsg() << std::endl;
    return -1;
  }
  if (validator_config.has_blob_storage_config()) {
    RunBlobStorageValidation(validator_config.blob_storage_config());
  }
  std::cout << "SUCCESS. Ran all validation tests. For individual statuses, "
               "see above."
            << std::endl;
  return 0;
}
