// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>
#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/container/node_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/log/initialize.h"
#include "absl/status/status.h"
#include "src/public/core/interface/errors.h"
#include "src/public/cpio/cli/blob_storage_cli.h"
#include "src/public/cpio/interface/cpio.h"

using google::scp::core::ExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::cpio::BlobStorageClientOptions;
using google::scp::cpio::Cpio;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::LogOption;
using google::scp::cpio::cli::CliBlobStorage;

ABSL_FLAG(std::string, project_id, "", "GCP Project ID or number");
ABSL_FLAG(std::string, region, "", "GCP Region, example: `us-east1`");

namespace {
constexpr std::string_view kUsageMessage = R"(
Usage: gcp_cpio_cli <client> <command> <flags>

Common Flags:
    [--project]   GCP Project ID or number.
    [--region]    GCP Region. Example: `us-east1`.

Clients:
- blob                      CPIO blob storage client.
  Commands:
  - get                     Gets a blob from Cloud Storage. rpc GetBlob.
    [--blob_paths]            One or more fully qualified GCS paths to the blob in the
                              format of `gs://<bucket_name>/<file_path_inside_bucket>`.

  - list                    Lists all blobs from Cloud Storage. rpc ListBlobsMetadata.
    [--blob_paths]            One or more fully qualified GCS paths to the blob in the
                              format of `gs://<bucket_name>/<file_path_inside_bucket>`.
                              Each --blob_paths will invoke ListBlobsMetadata separately.
    [--exclude_directories]   (Optional) Defaults to `false`. If true, exclude
                              blobs that are directories in ListBlobsMetadata response.

  - put                     Puts / uploads a blob to Cloud Storage. rpc PutBlob.
    [--blob_paths]            Only one fully qualified GCS path to the blob in the
                              format of `gs://<bucket_name>/<file_path_inside_bucket>`.
    [--blob_data]             String data of the blob to upload to Cloud Storage.

  - delete                  Deletes blobs in Cloud Storage. rpc DeleteBlob.
    [--blob_paths]            One or more fully qualified GCS paths to the blob in the
                              format of `gs://<bucket_name>/<file_path_inside_bucket>`.

  Examples:
    Please provide --project and --region with each request.

    (1) Retrieve a single blob from GCS.
    - gcp_cpio_cli blob get \
      --blob_paths gs://example_bucket/example_blob.txt

    (2) Retrieve multiple blobs from GCS.
    - gcp_cpio_cli blob get \
      --blob_paths gs://example_bucket/example_blob.txt \
      --blob_paths gs://example_bucket_2/example_blob.txt

    (3) List all blobs starting at a bucket or blob.
    - gcp_cpio_cli blob list \
      --blob_paths gs://example_bucket/example_blob.txt

    (4) List all blobs, excluding directory blobs, in a bucket and starting from a blob.
    - gcp_cpio_cli blob list \
      --blob_paths gs://example_bucket \
      --blob_paths gs://example_bucket/more_blobs_directory \
      --exclude_directories
    The expected return value of `gs://example_bucket` will not contain
    `gs://example_bucket/more_blobs_directory` and any of its blob contents.

    (5) Puts a blob to GCS with string text "example data".
    - gcp_cpio_cli blob put \
      --blob_paths gs://example_bucket/example_blob.txt \
      --blob_data "example data"

    (6) Delete two blobs in GCS.
    - gcp_cpio_cli blob delete \
      --blob_paths gs://example_bucket/example_blob.txt \
      --blob_paths gs://example_bucket/example_blob_2.txt

Try --help to see detailed available clients, client commands, and flag descriptions.
)";
const absl::NoDestructor<absl::node_hash_set<std::string_view>>
    kSupportedClients({
        CliBlobStorage::kClientName,
    });
}  // namespace

int main(int argc, char* argv[]) {
  // Initialize ABSL.
  absl::InitializeLog();
  absl::SetProgramUsageMessage(kUsageMessage);

  // Check client, command, and required flags are provided.
  const std::vector<char*> args = absl::ParseCommandLine(argc, argv);
  if (args.size() < 3) {
    std::cerr << "You must specify a client and command.\n"
              << absl::ProgramUsageMessage();
    return EXIT_FAILURE;
  }
  const std::string_view client_name{args[1]};
  if (!kSupportedClients->contains(client_name)) {
    std::cerr << "Client: [" << client_name << "] is not supported.\n"
              << absl::ProgramUsageMessage();
    return EXIT_FAILURE;
  }
  const std::string_view command{args[2]};
  std::string project_id = absl::GetFlag(FLAGS_project_id);
  if (project_id.empty()) {
    std::cerr << "Please provide --project_id" << std::endl;
    return EXIT_FAILURE;
  }
  std::string region = absl::GetFlag(FLAGS_region);
  if (region.empty()) {
    std::cerr << "Please provide --region" << std::endl;
    return EXIT_FAILURE;
  }

  // TODO: b/331941751 - Add ABSL flag for log_option.
  CpioOptions cpio_options;
  cpio_options.log_option = LogOption::kConsoleLog;
  if (google::scp::core::ExecutionResult error = Cpio::InitCpio(cpio_options);
      !error.Successful()) {
    std::cerr << "Failed to initialize CPIO: "
              << GetErrorMessage(error.status_code) << std::endl;
    return EXIT_FAILURE;
  }

  absl::Status cli_result;

  if (client_name == "blob") {
    BlobStorageClientOptions options = BlobStorageClientOptions();
    options.project_id = project_id;
    options.region = region;
    CliBlobStorage blob_storage_cli = CliBlobStorage(options);
    cli_result = blob_storage_cli.RunCli(command);
  }

  if (google::scp::core::ExecutionResult error =
          Cpio::ShutdownCpio(cpio_options);
      !error.Successful()) {
    std::cerr << "Failed to initialize CPIO: "
              << GetErrorMessage(error.status_code) << std::endl;
    return EXIT_FAILURE;
  }

  if (!cli_result.ok()) {
    std::cerr << cli_result << "\n" << absl::ProgramUsageMessage() << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Done :)" << std::endl;
  return EXIT_SUCCESS;
}
