/*
 * Copyright 2024 Google LLC
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
#ifndef CALLBACK_SERVER_IMPL_H_
#define CALLBACK_SERVER_IMPL_H_

#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include <grpcpp/grpcpp.h>

#include <apis/privacysandbox/apis/parc/v0/parc_service.grpc.pb.h>

#include "absl/container/flat_hash_map.h"
#include "azure/storage/blobs.hpp"

namespace privacysandbox::parc::azure {

class CallbackServerImpl
    : public privacysandbox::apis::parc::v0::ParcService::CallbackService {
 public:
  explicit CallbackServerImpl(
      absl::flat_hash_map<std::string, std::string> parameters,
      Azure::Storage::Blobs::BlobServiceClient blob_service_client,
      int64_t blob_chunk_size, int64_t chunk_buffer_size)
      : parameters_(std::move(parameters)),
        blob_service_client_(std::move(blob_service_client)),
        blob_chunk_size_(blob_chunk_size),
        chunk_buffer_size_(chunk_buffer_size) {}

  // Not copyable or movable.
  CallbackServerImpl(const CallbackServerImpl&) = delete;
  CallbackServerImpl& operator=(const CallbackServerImpl&) = delete;
  ~CallbackServerImpl() override = default;

  static absl::StatusOr<std::unique_ptr<CallbackServerImpl>> Create(
      const std::filesystem::path& parameters_file_path,
      const bool use_workload_auth, const std::string account_name,
      const std::string account_key, const std::string blob_endpoint,
      const std::string certs_dir, int64_t blob_chunk_size,
      int64_t chunk_buffer_size);

  grpc::ServerUnaryReactor* GetParameter(
      grpc::CallbackServerContext* context,
      const privacysandbox::apis::parc::v0::GetParameterRequest* request,
      privacysandbox::apis::parc::v0::GetParameterResponse* response) override;

  grpc::ServerUnaryReactor* ListBlobsMetadata(
      grpc::CallbackServerContext* context,
      const privacysandbox::apis::parc::v0::ListBlobsMetadataRequest* request,
      privacysandbox::apis::parc::v0::ListBlobsMetadataResponse* response)
      override;

  grpc::ServerUnaryReactor* GetBlobMetadata(
      grpc::CallbackServerContext* context,
      const privacysandbox::apis::parc::v0::GetBlobMetadataRequest* request,
      privacysandbox::apis::parc::v0::GetBlobMetadataResponse* response)
      override;
  grpc::ServerWriteReactor<privacysandbox::apis::parc::v0::GetBlobResponse>*
  GetBlob(
      grpc::CallbackServerContext* context,
      const privacysandbox::apis::parc::v0::GetBlobRequest* request) override;

 private:
  absl::flat_hash_map<std::string, std::string> parameters_;
  Azure::Storage::Blobs::BlobServiceClient blob_service_client_;
  int64_t blob_chunk_size_;
  int64_t chunk_buffer_size_;
};

}  // namespace privacysandbox::parc::azure

#endif  // CALLBACK_SERVER_IMPL_H_
