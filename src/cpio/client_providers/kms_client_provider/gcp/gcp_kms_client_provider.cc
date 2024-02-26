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

#include "gcp_kms_client_provider.h"

#include <memory>
#include <string_view>
#include <utility>

#include <tink/aead.h>

#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/cpio/interface/kms_client/type_def.h"

#include "error_codes.h"
#include "gcp_key_management_service_client.h"
#include "gcp_kms_aead.h"

using crypto::tink::Aead;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED;
using google::scp::core::errors::
    SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::utils::Base64Decode;

namespace {
/// Filename for logging errors
constexpr std::string_view kGcpKmsClientProvider = "GcpKmsClientProvider";
}  // namespace

namespace google::scp::cpio::client_providers {

ExecutionResult GcpKmsClientProvider::Init() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Run() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Stop() noexcept {
  return SuccessExecutionResult();
}

ExecutionResult GcpKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_CIPHERTEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.Finish(execution_result);
    return decrypt_context.result;
  }

  const auto& key_arn = decrypt_context.request->key_resource_name();
  if (key_arn.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to get Key Arn from decryption request.");
    decrypt_context.Finish(execution_result);
    return decrypt_context.result;
  }

  auto aead_or =
      aead_provider_->CreateAead(decrypt_context.request->gcp_wip_provider(),
                                 decrypt_context.request->account_identity(),
                                 decrypt_context.request->key_resource_name());
  if (!aead_or.Successful()) {
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, aead_or.result(),
                      "Failed to get Aead.");
    decrypt_context.Finish(aead_or.result());
    return decrypt_context.result;
  }

  std::string decoded_ciphertext;
  auto execution_result = Base64Decode(ciphertext, decoded_ciphertext);
  if (!execution_result.Successful()) {
    auto execution_result = FailureExecutionResult(
        SC_GCP_KMS_CLIENT_PROVIDER_BASE64_DECODING_FAILED);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Failed to decode the ciphertext using base64.");
    decrypt_context.Finish(execution_result);
    return decrypt_context.result;
  }

  auto decrypt_or =
      (*aead_or)->Decrypt(decoded_ciphertext, "" /*associated_data*/);
  if (!decrypt_or.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
    SCP_ERROR_CONTEXT(kGcpKmsClientProvider, decrypt_context, execution_result,
                      "Aead Decryption failed with error %s.",
                      decrypt_or.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return decrypt_context.result;
  }
  decrypt_context.response = std::make_shared<DecryptResponse>();
  decrypt_context.response->set_plaintext(std::move(*decrypt_or));
  decrypt_context.Finish(SuccessExecutionResult());
  return SuccessExecutionResult();
}

ExecutionResultOr<std::shared_ptr<Aead>> GcpKmsAeadProvider::CreateAead(
    std::string_view wip_provider,
    std::string_view service_account_to_impersonate,
    std::string_view key_arn) noexcept {
  auto key_management_service_client = CreateKeyManagementServiceClient(
      wip_provider, service_account_to_impersonate);
  auto gcp_key_management_service_client =
      std::make_shared<GcpKeyManagementServiceClient>(
          key_management_service_client);

  auto aead_result =
      GcpKmsAead::New(key_arn, gcp_key_management_service_client);
  if (!aead_result.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_GCP_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR(kGcpKmsClientProvider, kZeroUuid, execution_result,
              "Failed to get Key Arn.");
    return execution_result;
  }
  return std::move(*aead_result);
}

std::unique_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    KmsClientOptions options,
    RoleCredentialsProviderInterface* role_credentials_provider,
    AsyncExecutorInterface* io_async_executor) noexcept {
  return std::make_unique<GcpKmsClientProvider>();
}
}  // namespace google::scp::cpio::client_providers
