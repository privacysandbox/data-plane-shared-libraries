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

#include "src/cpio/client_providers/kms_client_provider/gcp/gcp_kms_aead.h"

#include <memory>
#include <string>

#include <tink/aead.h>
#include <tink/util/status.h>
#include <tink/util/statusor.h>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/cloud/kms/key_management_client.h"
#include "google/cloud/kms/v1/service.grpc.pb.h"
#include "src/cpio/client_providers/kms_client_provider/gcp/gcp_key_management_service_client.h"

using crypto::tink::Aead;
using crypto::tink::util::Status;
using crypto::tink::util::StatusOr;
using google::cloud::kms::KeyManagementServiceClient;
using google::cloud::kms::v1::DecryptRequest;

namespace google::scp::cpio::client_providers {
GcpKmsAead::GcpKmsAead(
    std::string_view key_name,
    std::shared_ptr<GcpKeyManagementServiceClientInterface> kms_client)
    : key_name_(key_name), kms_client_(kms_client) {}

StatusOr<std::unique_ptr<Aead>> GcpKmsAead::New(
    std::string_view key_name,
    std::shared_ptr<GcpKeyManagementServiceClientInterface> kms_client) {
  if (key_name.empty()) {
    return Status(absl::StatusCode::kInvalidArgument,
                  "Key name cannot be empty.");
  }
  if (!kms_client) {
    return Status(absl::StatusCode::kInvalidArgument,
                  "KMS client cannot be null.");
  }
  return std::unique_ptr<Aead>(new GcpKmsAead(key_name, kms_client));
}

StatusOr<std::string> GcpKmsAead::Encrypt(
    std::string_view plaintext, std::string_view associated_data) const {
  return Status(absl::StatusCode::kUnimplemented,
                "GCP KMS encryption unimplemented");
}

StatusOr<std::string> GcpKmsAead::Decrypt(
    std::string_view ciphertext, std::string_view associated_data) const {
  DecryptRequest req;
  req.set_name(key_name_);
  req.set_ciphertext(std::string(ciphertext));
  req.set_additional_authenticated_data(std::string(associated_data));
  auto response = kms_client_->Decrypt(req);

  if (!response) {
    return Status(absl::StatusCode::kInvalidArgument,
                  absl::StrCat("GCP KMS decryption failed: ",
                               response.status().message()));
  }

  return response->plaintext();
}

}  // namespace google::scp::cpio::client_providers
