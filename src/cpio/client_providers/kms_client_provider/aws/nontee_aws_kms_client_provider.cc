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

#include "nontee_aws_kms_client_provider.h"

#include <memory>
#include <string_view>
#include <utility>

#include <aws/core/auth/AWSCredentialsProvider.h>
#include <aws/core/auth/AWSCredentialsProviderChain.h>

#include "absl/functional/bind_front.h"
#include "src/core/async_executor/aws/aws_async_executor.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/cpio/common/aws/aws_utils.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/kms_client/type_def.h"

#include "aws_kms_aead.h"
#include "nontee_error_codes.h"

using Aws::Auth::AWSCredentials;
using Aws::Client::ClientConfiguration;
using Aws::KMS::KMSClient;
using crypto::tink::Aead;
using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::async_executor::aws::AwsAsyncExecutor;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED;
using google::scp::core::errors::
    SC_AWS_KMS_CLIENT_PROVIDER_CREDENTIALS_PROVIDER_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND;
using google::scp::core::errors::SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::utils::Base64Decode;
using google::scp::cpio::common::CreateClientConfiguration;

namespace {
/// Filename for logging errors
constexpr std::string_view kNonteeAwsKmsClientProvider =
    "NonteeAwsKmsClientProvider";
}  // namespace

namespace google::scp::cpio::client_providers {

absl::Status NonteeAwsKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  const auto& key_arn = decrypt_context.request->key_resource_name();
  if (key_arn.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_KEY_ARN_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key Arn from decryption request.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  const auto& kms_region = decrypt_context.request->kms_region();
  if (kms_region.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get Key Region from decryption request.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  const auto& account_identity = decrypt_context.request->account_identity();
  if (account_identity.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
    SCP_ERROR_CONTEXT(
        kNonteeAwsKmsClientProvider, decrypt_context, execution_result,
        "Failed to get Account Identity from decryption request.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  AsyncContext<DecryptRequest, Aead> get_aead_context(
      decrypt_context.request,
      absl::bind_front(&NonteeAwsKmsClientProvider::GetAeadCallbackToDecrypt,
                       this, decrypt_context),
      decrypt_context);
  if (const ExecutionResult result = GetAead(get_aead_context);
      !result.Successful()) {
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(result.status_code));
  }
  return absl::OkStatus();
}

ExecutionResult NonteeAwsKmsClientProvider::GetAeadCallbackToDecrypt(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    AsyncContext<DecryptRequest, Aead>& get_aead_context) noexcept {
  auto execution_result = get_aead_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get Aead.");
    decrypt_context.Finish(execution_result);
    return decrypt_context.result;
  }

  std::string decoded_ciphertext;
  Base64Decode(decrypt_context.request->ciphertext(), decoded_ciphertext);

  auto decrypt_result =
      get_aead_context.response->Decrypt(decoded_ciphertext, "");
  if (!decrypt_result.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_DECRYPTION_FAILED);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Aead Decryption failed with error %s.",
                      decrypt_result.status().ToString().c_str());
    decrypt_context.Finish(execution_result);
    return decrypt_context.result;
  }
  decrypt_context.response = std::make_shared<DecryptResponse>();
  decrypt_context.response->set_plaintext(std::move(*decrypt_result));
  decrypt_context.Finish(SuccessExecutionResult());
  return SuccessExecutionResult();
}

ExecutionResult NonteeAwsKmsClientProvider::GetAead(
    AsyncContext<DecryptRequest, Aead>& get_aead_context) noexcept {
  AsyncContext<DecryptRequest, KMSClient> create_kms_context(
      get_aead_context.request,
      absl::bind_front(
          &NonteeAwsKmsClientProvider::CreateKmsCallbackToCreateAead, this,
          get_aead_context),
      get_aead_context);
  return CreateKmsClient(create_kms_context);
}

void NonteeAwsKmsClientProvider::CreateKmsCallbackToCreateAead(
    AsyncContext<DecryptRequest, Aead>& get_aead_context,
    AsyncContext<DecryptRequest, KMSClient>& create_kms_context) noexcept {
  auto execution_result = create_kms_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, get_aead_context,
                      execution_result, "Failed to create KMS Client.");
    get_aead_context.Finish(execution_result);
    return;
  }

  auto aead_result =
      AwsKmsAead::New(get_aead_context.request->key_resource_name(),
                      std::move(create_kms_context.response));
  if (!aead_result.ok()) {
    auto execution_result =
        FailureExecutionResult(SC_AWS_KMS_CLIENT_PROVIDER_CREATE_AEAD_FAILED);
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider, get_aead_context,
                      execution_result, "Failed to get Key Arn.");
    get_aead_context.Finish(execution_result);
    return;
  }
  get_aead_context.response = std::move(*aead_result);
  get_aead_context.Finish(SuccessExecutionResult());
}

ExecutionResult NonteeAwsKmsClientProvider::CreateKmsClient(
    AsyncContext<DecryptRequest, KMSClient>& create_kms_context) noexcept {
  auto request = std::make_shared<GetRoleCredentialsRequest>();
  request->account_identity = std::make_shared<AccountIdentity>(
      create_kms_context.request->account_identity());
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_role_credentials_context(
          std::move(request),
          absl::bind_front(&NonteeAwsKmsClientProvider::
                               GetSessionCredentialsCallbackToCreateKms,
                           this, create_kms_context),
          create_kms_context);
  return role_credentials_provider_
                 ->GetRoleCredentials(get_role_credentials_context)
                 .ok()
             ? SuccessExecutionResult()
             : FailureExecutionResult(SC_UNKNOWN);
}

void NonteeAwsKmsClientProvider::GetSessionCredentialsCallbackToCreateKms(
    AsyncContext<DecryptRequest, KMSClient>& create_kms_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kNonteeAwsKmsClientProvider,
                      get_session_credentials_context, execution_result,
                      "Failed to get AWS Credentials.");
    create_kms_context.Finish(execution_result);
    return;
  }

  const GetRoleCredentialsResponse& response =
      *get_session_credentials_context.response;
  auto aws_credentials = std::make_shared<AWSCredentials>(
      response.access_key_id->c_str(), response.access_key_secret->c_str(),
      response.security_token->c_str());

  auto kms_client = GetKmsClient(
      std::move(aws_credentials),
      std::make_shared<std::string>(create_kms_context.request->kms_region()));
  create_kms_context.response = kms_client;
  create_kms_context.Finish(SuccessExecutionResult());
}

ClientConfiguration NonteeAwsKmsClientProvider::CreateClientConfiguration(
    std::string_view region) noexcept {
  auto client_config = common::CreateClientConfiguration(std::string(region));
  client_config.executor =
      std::make_shared<AwsAsyncExecutor>(io_async_executor_);
  return client_config;
}

std::shared_ptr<KMSClient> NonteeAwsKmsClientProvider::GetKmsClient(
    const std::shared_ptr<AWSCredentials>& aws_credentials,
    const std::shared_ptr<std::string>& kms_region) noexcept {
  return std::make_shared<KMSClient>(*aws_credentials,
                                     CreateClientConfiguration(*kms_region));
}

std::unique_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    absl::Nonnull<RoleCredentialsProviderInterface*> role_credentials_provider,
    core::AsyncExecutorInterface* io_async_executor) noexcept {
  return std::make_unique<NonteeAwsKmsClientProvider>(role_credentials_provider,
                                                      io_async_executor);
}
}  // namespace google::scp::cpio::client_providers
