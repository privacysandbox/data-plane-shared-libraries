/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_CPIO_CLI_BLOB_STORAGE_CLI_H_
#define PUBLIC_CPIO_CLI_BLOB_STORAGE_CLI_H_

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"

namespace google::scp::cpio::cli {

/**
 * @brief To initialize, run, and shutdown the CPIO Blob Storage client for the
 * CLI.
 */
class CliBlobStorage final {
 public:
  explicit CliBlobStorage(google::scp::cpio::BlobStorageClientOptions options)
      : blob_storage_client_(
            google::scp::cpio::BlobStorageClientFactory::Create(
                std::move(options))) {
    if (absl::Status error = blob_storage_client_->Init(); !error.ok()) {
      std::cerr << "Failed to Init BlobStorageClient: " << error << std::endl;
      exit(EXIT_FAILURE);
    }
  }

  /**
   * @brief Parses the command line flags and runs the blob client.
   *
   * @param command The input command from the command line.
   */
  absl::Status RunCli(std::string_view command);

  /**
   * @brief Calls GetBlob on the BlobStorageClient.
   *
   * @param bucket_name The name of the bucket of the blob to be retrieved.
   * @param blob_name The name of the blob to retrieve.
   */
  absl::Status GetBlob(std::string_view bucket_name,
                       std::string_view blob_name);

  /**
   * @brief Calls ListBlobs on the BlobStorageClient.
   *
   * @param bucket_name The name of the bucket to list all blobs at.
   * @param blob_name Optionally, a blob subdirectory to list all blobs at.
   * @param exclude_directories Optionally, exclude blob subdirectories from
   * list output.
   */
  absl::Status ListBlobs(std::string_view bucket_name,
                         std::string_view blob_name, bool exclude_directories);

  /**
   * @brief Calls PutBlob on the BlobStorageClient.
   *
   * @param bucket_name The name of the bucket of the blob.
   * @param blob_name The name of the blob to put data at.
   * @param blob_data The string data of the blob.
   */
  absl::Status PutBlob(std::string_view bucket_name, std::string_view blob_name,
                       std::string_view blob_data);

  /**
   * @brief Calls DeleteBlob on the BlobStorageClient.
   *
   * @param bucket_name The name of the bucket of the blob to be deleted.
   * @param blob_name The name of the blob to delete.
   */
  absl::Status DeleteBlob(std::string_view bucket_name,
                          std::string_view blob_name);

  static constexpr std::string_view kClientName = "blob";

 private:
  std::unique_ptr<google::scp::cpio::BlobStorageClientInterface>
      blob_storage_client_;
};
};  // namespace google::scp::cpio::cli
#endif  // PUBLIC_CPIO_CLI_BLOB_STORAGE_CLI_H_
