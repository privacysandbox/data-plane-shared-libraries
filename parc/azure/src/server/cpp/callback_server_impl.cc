// Copyright 2025 Google LLC
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

#include "src/server/cpp/callback_server_impl.h"

#include <pthread.h>

#include <algorithm>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <apis/privacysandbox/apis/parc/v0/parc_service.grpc.pb.h>
#include <google/protobuf/util/json_util.h>

#include "absl/container/flat_hash_map.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "azure/core/credentials/credentials.hpp"
#include "azure/core/http/curl_transport.hpp"
#include "azure/identity/workload_identity_credential.hpp"
#include "azure/storage/blobs.hpp"
#include "azure/storage/blobs/blob_options.hpp"
#include "azure/storage/common/crypt.hpp"
#include "src/proto/parameter.pb.h"
#include "src/server/cpp/utils/blocking_bounded_queue.h"
#include "src/server/cpp/utils/status_util.h"

namespace {

absl::StatusOr<absl::flat_hash_map<std::string, std::string>>
ParseParametersFile(const std::filesystem::path& parameters_file_path) {
  std::ifstream file_stream;
  file_stream.open(parameters_file_path);

  absl::flat_hash_map<std::string, std::string> parameters;

  if (!file_stream.is_open()) {
    return absl::NotFoundError(
        "Error on startup! Cannot find parameters file!");
  } else {
    std::string line;
    while (std::getline(file_stream, line)) {
      privacysandbox::parc::Parameter parameter;
      if (const auto status =
              google::protobuf::util::JsonStringToMessage(line, &parameter);
          status.ok()) {
        parameters[parameter.key()] = parameter.value();
      } else {
        return absl::InvalidArgumentError(absl::StrCat(
            "Proto deserializing failed! Message: ", status.message()));
      }
    }
  }
  return parameters;
}

class GetBlobReactor : public grpc::ServerWriteReactor<
                           privacysandbox::apis::parc::v0::GetBlobResponse> {
 public:
  GetBlobReactor(const privacysandbox::apis::parc::v0::GetBlobRequest* request,
                 Azure::Storage::Blobs::BlobServiceClient* client,
                 int64_t chunk_size, int64_t chunk_buffer_size)
      : client_(client),
        chunk_size_(chunk_size),
        expected_length_(0),
        responses_(chunk_buffer_size) {
    if (request == nullptr || !request->has_blob_metadata()) {
      Finish(
          grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                       absl::StrCat("GetBlob: blob_metadata not specified.")));
      return;
    }
    bucket_name_ = request->blob_metadata().bucket_name();
    blob_name_ = request->blob_metadata().blob_name();
    if (bucket_name_.empty() && blob_name_.empty()) {
      Finish(grpc::Status(
          grpc::StatusCode::INVALID_ARGUMENT,
          "GetBlob: bucket_name and blob_name fields both unspecified."));
      return;
    }

    try {
      Azure::Storage::Blobs::BlobContainerClient container_client =
          client_->GetBlobContainerClient(bucket_name_);
      Azure::Storage::Blobs::BlobClient blob_client =
          container_client.GetBlobClient(blob_name_);
      LOG(INFO) << "Attempting to download blob " << blob_name_ << ": "
                << blob_client.GetUrl();
      Azure::Storage::Blobs::DownloadBlobOptions opts;
      if (request->has_byte_range()) {
        // Validate end is not less than begin.
        if (request->byte_range().end() < request->byte_range().begin()) {
          Finish(grpc::Status(
              grpc::StatusCode::INVALID_ARGUMENT,
              absl::StrCat("GetBlob: Provided byte_range byte index end [",
                           request->byte_range().end(),
                           "] is less than byte index begin [",
                           request->byte_range().begin(), "].")));
          return;
        }
        expected_length_ =
            request->byte_range().end() - request->byte_range().begin() + 1;
        current_offset_ = request->byte_range().begin();
        opts.Range = Azure::Core::Http::HttpRange{
            .Offset = static_cast<int64_t>(request->byte_range().begin()),
            .Length = expected_length_,
        };
      }

      Azure::Response<Azure::Storage::Blobs::Models::DownloadBlobResult>
          download = blob_client.Download(opts);
      if (const int response_status_code =
              static_cast<int>(download.RawResponse->GetStatusCode());
          response_status_code >= 400) {
        LOG(ERROR) << "download status code: " << response_status_code;
        Finish(grpc::Status(
            grpc::StatusCode::INVALID_ARGUMENT,
            absl::StrCat("GetBlob: Error downloading blob. Status code: ",
                         response_status_code, ", Reason: ",
                         download.RawResponse->GetReasonPhrase())));
        return;
      }
      Azure::Storage::Blobs::Models::DownloadBlobResult& blobinfo =
          download.Value;
      blob_stream_ = std::move(blobinfo.BodyStream);
      if (expected_length_ == 0) {
        current_offset_ = 0;
        expected_length_ = blobinfo.BlobSize;
      } else {
        expected_length_ =
            std::min(expected_length_, blobinfo.BlobSize - current_offset_);
      }
      end_offset_ = current_offset_ + expected_length_;

      LOG(INFO) << "GetBlob:"
                << " ETag: " << blobinfo.Details.ETag.ToString()
                << " Last Modified: "
                << blobinfo.Details.LastModified.ToString()
                << " Content-Type: " << blobinfo.Details.HttpHeaders.ContentType
                << " Content-Encoding: "
                << blobinfo.Details.HttpHeaders.ContentEncoding
                << " Size: " << blobinfo.BlobSize << " bytes"
                << " Download range: " << expected_length_ << " bytes";

      read_thread_ = std::thread([&] { StreamReadBlob(); });
      NextWrite();
    } catch (const Azure::Storage::StorageException& e) {
      LOG(INFO) << "ERROR: Azure Storage Exception: " << e.what();
      switch (e.StatusCode) {
        case Azure::Core::Http::HttpStatusCode::NotFound: {
          const std::string mesg =
              absl::StrCat("GetBlob: Blob not found: container: ", bucket_name_,
                           ", blob: ", blob_name_);
          LOG(INFO) << mesg << (e.Message.empty() ? e.ReasonPhrase : e.Message);
          Finish(grpc::Status(grpc::StatusCode::NOT_FOUND, mesg));
          break;
        }
        case Azure::Core::Http::HttpStatusCode::Forbidden:
        case Azure::Core::Http::HttpStatusCode::Unauthorized: {
          const std::string mesg =
              absl::StrCat("GetBlob: Permission denied accessing blob");
          LOG(INFO) << mesg << ": "
                    << (e.Message.empty() ? e.ReasonPhrase : e.Message);
          Finish(grpc::Status(grpc::StatusCode::PERMISSION_DENIED, mesg));
          break;
        }
        case Azure::Core::Http::HttpStatusCode::RangeNotSatisfiable: {
          const std::string mesg =
              absl::StrCat("GetBlob: Invalid range requested for blob");
          LOG(INFO) << mesg << ": "
                    << (e.Message.empty() ? e.ReasonPhrase : e.Message);
          Finish(grpc::Status(grpc::StatusCode::OUT_OF_RANGE, mesg));
          break;
        }
        default:
          LOG(INFO) << "Azure Storage error" << e.what();
          Finish(grpc::Status(grpc::StatusCode::NOT_FOUND,
                              "GetBlob: Storage error"));
          break;
      }
    } catch (const std::exception& e) {
      LOG(ERROR) << "Standard Exception during download setup: " << e.what();
      Finish(grpc::Status(
          grpc::StatusCode::INTERNAL,
          "GetBlob: Internal server error during blob download setup"));
    } catch (...) {
      LOG(ERROR) << "Unknown exception during download setup.";
      Finish(grpc::Status(
          grpc::StatusCode::UNKNOWN,
          "GetBlob: Unknown server error during blob download setup"));
    }
  }

  ~GetBlobReactor() override = default;

 private:
  void NextWrite() {
    if (cancel_.HasBeenNotified()) {
      return;
    }
    current_response_ = responses_.Take();
    // ServerMetrics::Add(kServerMetricGetBlobBytesSent,
    //                    current_response_.data().size());
    StartWrite(&current_response_);
  }

  void StreamReadBlob() {
    bool first_response = true;
    while (current_offset_ < end_offset_) {
      if (cancel_.HasBeenNotified()) {
        return;
      }
      privacysandbox::apis::parc::v0::GetBlobResponse response;
      if (first_response) {
        // Only return blob metadata and content encoding on first response.
        response.mutable_blob_metadata()->set_bucket_name(bucket_name_);
        response.mutable_blob_metadata()->set_blob_name(blob_name_);
        response.set_content_encoding(
            privacysandbox::apis::parc::v0::CONTENT_ENCODING_PLAINTEXT);
        first_response = false;
      }
      if (absl::Status status = ReadChunk(response); !status.ok()) {
        LOG(WARNING) << status;
        Finish(privacysandbox::parc::utils::FromAbslStatus(
            absl::UnknownError("GetBlob: reading blob data")));
        return;
      }
      responses_.Put(std::move(response));
    }
    LOG(INFO) << "Finished reading blob stream";
  }

  absl::Status ReadChunk(
      privacysandbox::apis::parc::v0::GetBlobResponse& response) {
    const int64_t remaining = end_offset_ - current_offset_;
    int64_t chunk_size = std::min(remaining, chunk_size_);
    if (chunk_size < 0) {
      return absl::InternalError("blob retrieval chunk size is negative");
    }
    if (chunk_size == 0) {
      return absl::OkStatus();
    }
    auto& blob_data = *response.mutable_data();
    blob_data.resize(chunk_size);

    int64_t chunk_read = 0;
    while (chunk_read < chunk_size) {
      uint8_t* offset_ptr =
          reinterpret_cast<uint8_t*>(blob_data.data()) + chunk_read;
      const int64_t bytes_read =
          blob_stream_->Read(offset_ptr, chunk_size - chunk_read,
                             Azure::Core::Context::ApplicationContext);
      if (bytes_read == 0) {
        return absl::ResourceExhaustedError(
            "blob retrieval stream ended prematurely");
      }
      LOG(INFO) << "bytes read: " << bytes_read;
      chunk_read += bytes_read;
    }
    current_offset_ += chunk_read;
    blob_data.resize(chunk_read);
    return absl::OkStatus();
  }

  void OnWriteDone(bool ok) override {
    if (!ok) {
      cancel_.Notify();
      Finish(
          grpc::Status(grpc::StatusCode::INTERNAL,
                       "GetBlob: gRPC writing failed for an unknown reason."));
    } else if (current_offset_ == end_offset_ && responses_.IsEmpty()) {
      Finish(grpc::Status::OK);
    } else {
      NextWrite();
    }
  }

  void OnCancel() override {
    LOG(WARNING) << "GetBlob RPC Cancelled.";
    cancel_.Notify();
  }

  void OnDone() override {
    if (read_thread_.joinable()) {
      read_thread_.join();
    }
    delete this;
  }

  Azure::Storage::Blobs::BlobServiceClient* client_;
  std::unique_ptr<Azure::Core::IO::BodyStream> blob_stream_;
  std::string bucket_name_;
  std::string blob_name_;
  int64_t chunk_size_;
  int64_t expected_length_;
  int64_t current_offset_;
  privacysandbox::apis::parc::v0::GetBlobResponse current_response_;
  int64_t end_offset_;  // One beyond the range end (half-open)
  privacysandbox::parc::utils::BlockingBoundedQueue<
      privacysandbox::apis::parc::v0::GetBlobResponse>
      responses_;
  absl::Notification cancel_;
  std::thread read_thread_;
};

}  // namespace

namespace privacysandbox::parc::azure {
absl::StatusOr<std::unique_ptr<CallbackServerImpl>> CallbackServerImpl::Create(
    const std::filesystem::path& parameters_file_path,
    const bool use_workload_auth, const std::string account_name,
    const std::string account_key, const std::string blob_endpoint,
    const std::string certs_dir, int64_t blob_chunk_size,
    int64_t chunk_buffer_size = 3) {
  auto params = ParseParametersFile(parameters_file_path);
  if (!params.ok()) {
    return params.status();
  }

  if (blob_endpoint.empty()) {
    return absl::UnavailableError("Blob endpoint is required");
  }

  Azure::Storage::Blobs::BlobClientOptions client_opts;
  client_opts.Transport.Transport =
      std::make_shared<Azure::Core::Http::CurlTransport>();
  if (use_workload_auth) {
    LOG(INFO) << "Using workload identity credentials";
    Azure::Core::Http::CurlTransportOptions curl_opts;
    curl_opts.CAPath = certs_dir;
    Azure::Identity::WorkloadIdentityCredentialOptions cred_opts;
    cred_opts.Transport.Transport =
        std::make_shared<Azure::Core::Http::CurlTransport>(curl_opts);
    Azure::Storage::Blobs::BlobServiceClient blob_service_client(
        blob_endpoint,
        /*credential=*/
        std::make_shared<Azure::Identity::WorkloadIdentityCredential>(
            cred_opts),
        std::move(client_opts));
    return std::make_unique<CallbackServerImpl>(
        *params, std::move(blob_service_client), blob_chunk_size,
        chunk_buffer_size);
  } else {
    if (account_name.empty() || account_key.empty()) {
      return absl::UnavailableError(
          "Must provide account name and account key if using Storage "
          "SharedKey auth");
    }
    LOG(INFO) << "Using Storage SharedKey credentials";
    Azure::Storage::Blobs::BlobServiceClient blob_service_client(
        absl::StrCat(blob_endpoint, "/", account_name),
        /*credential=*/
        std::make_shared<Azure::Storage::StorageSharedKeyCredential>(
            account_name, account_key),
        client_opts);
    return std::make_unique<CallbackServerImpl>(
        *params, std::move(blob_service_client), blob_chunk_size,
        chunk_buffer_size);
  }
}

grpc::ServerUnaryReactor* CallbackServerImpl::GetParameter(
    grpc::CallbackServerContext* context,
    const privacysandbox::apis::parc::v0::GetParameterRequest* request,
    privacysandbox::apis::parc::v0::GetParameterResponse* response) {
  LOG(INFO) << "GetParameter: " << request->ShortDebugString();
  auto* reactor = context->DefaultReactor();

  if (parameters_.contains(request->parameter_name())) {
    response->set_parameter_value(parameters_.at(request->parameter_name()));
    reactor->Finish(grpc::Status::OK);
  } else {
    reactor->Finish(
        grpc::Status(grpc::StatusCode::NOT_FOUND, "Parameter not found"));
  }
  return reactor;
}

grpc::ServerUnaryReactor* CallbackServerImpl::ListBlobsMetadata(
    grpc::CallbackServerContext* context,
    const privacysandbox::apis::parc::v0::ListBlobsMetadataRequest* request,
    privacysandbox::apis::parc::v0::ListBlobsMetadataResponse* response) {
  auto* reactor = context->DefaultReactor();

  if (!request->has_blob_metadata() ||
      request->blob_metadata().bucket_name().empty()) {
    reactor->Finish(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "ListBlobsMetadata: Must specify container name"));
    return reactor;
  }
  Azure::Storage::Blobs::ListBlobsOptions opts;
  if (std::string blob_prefix = request->blob_metadata().blob_name();
      !blob_prefix.empty()) {
    opts.Prefix = blob_prefix;
  }
  if (request->has_max_page_size()) {
    uint64_t pgsize = request->max_page_size();
    if (pgsize == 0) {
      reactor->Finish(
          grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                       "ListBlobsMetadata: max page size cannot be zero"));
      return reactor;
    } else {
      opts.PageSizeHint = pgsize;
    }
  }
  if (std::string pgtok = request->page_token(); !pgtok.empty()) {
    opts.ContinuationToken = pgtok;
  }

  try {
    Azure::Storage::Blobs::BlobContainerClient container_client =
        blob_service_client_.GetBlobContainerClient(
            request->blob_metadata().bucket_name());
    Azure::Storage::Blobs::ListBlobsPagedResponse page =
        container_client.ListBlobs(opts);
    LOG(INFO) << "ListBlobs request succeeded";
    if (page.NextPageToken.HasValue()) {
      response->set_next_page_token(page.NextPageToken.Value());
    }
    for (auto& blob : page.Blobs) {
      LOG(INFO) << "Found blob! " << blob.Name;
      privacysandbox::apis::parc::v0::BlobMetadata* blob_metadata =
          response->add_blob_metadatas();
      blob_metadata->set_bucket_name(request->blob_metadata().bucket_name());
      blob_metadata->set_blob_name(blob.Name);
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "ListBlobsMetadata error: " << e.what();
    reactor->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                 "ListBlobsMetadata: Unspecified error"));
    return reactor;
  }
  reactor->Finish(grpc::Status::OK);
  return reactor;
}

grpc::ServerUnaryReactor* CallbackServerImpl::GetBlobMetadata(
    grpc::CallbackServerContext* context,
    const privacysandbox::apis::parc::v0::GetBlobMetadataRequest* request,
    privacysandbox::apis::parc::v0::GetBlobMetadataResponse* response) {
  LOG(INFO) << "GetBlobMetadata: " << request->ShortDebugString();
  auto* reactor = context->DefaultReactor();

  if (!request->has_blob_metadata() ||
      request->blob_metadata().bucket_name().empty() ||
      request->blob_metadata().blob_name().empty()) {
    reactor->Finish(
        grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                     "Bucket name and blob name must be specified"));
    return reactor;
  }
  Azure::Storage::Blobs::BlobClient blob_client =
      blob_service_client_
          .GetBlobContainerClient(request->blob_metadata().bucket_name())
          .GetBlobClient(request->blob_metadata().blob_name());

  try {
    auto blob_client_response = blob_client.GetProperties();
    auto* blob_metadata = response->mutable_blob_metadata();
    blob_metadata->set_bucket_name(request->blob_metadata().bucket_name());
    blob_metadata->set_blob_name(request->blob_metadata().blob_name());
    blob_metadata->set_blob_size(blob_client_response.Value.BlobSize);
    Azure::Storage::ContentHash content_hash =
        blob_client_response.Value.HttpHeaders.ContentHash;
    if (content_hash.Algorithm == Azure::Storage::HashAlgorithm::Md5) {
      auto* md5_hash = blob_metadata->add_hashes();
      md5_hash->set_type(
          privacysandbox::apis::parc::v0::HashType::HASH_TYPE_MD5);
      const std::string_view hash_val(
          reinterpret_cast<const char*>(content_hash.Value.data()),
          content_hash.Value.size());
      md5_hash->set_hash(absl::BytesToHexString(hash_val));
    }
    reactor->Finish(grpc::Status::OK);
  } catch (const std::exception& e) {
    LOG(ERROR) << "GetBlobMetadata Error: " << e.what();
    reactor->Finish(grpc::Status(grpc::StatusCode::INVALID_ARGUMENT,
                                 "GetBlobMetadata: Unspecified error"));
  }
  return reactor;
}

grpc::ServerWriteReactor<privacysandbox::apis::parc::v0::GetBlobResponse>*
CallbackServerImpl::GetBlob(
    grpc::CallbackServerContext* context,
    const privacysandbox::apis::parc::v0::GetBlobRequest* request) {
  // ServerMetrics::Add(kServerMetricGetBlobCount, 1);
  return new GetBlobReactor(request, &blob_service_client_, blob_chunk_size_,
                            chunk_buffer_size_);
}

}  // namespace privacysandbox::parc::azure
