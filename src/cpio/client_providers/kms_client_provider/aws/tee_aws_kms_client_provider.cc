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

#include "tee_aws_kms_client_provider.h"

#include <cstdio>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/functional/bind_front.h"
#include "absl/status/statusor.h"
#include "absl/strings/strip.h"
#include "src/core/utils/base64.h"
#include "src/cpio/client_providers/interface/role_credentials_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/kms_client/type_def.h"

#include "tee_aws_kms_client_provider_utils.h"
#include "tee_error_codes.h"

using google::cmrt::sdk::kms_service::v1::DecryptRequest;
using google::cmrt::sdk::kms_service::v1::DecryptResponse;
using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_CREDENTIAL_PROVIDER_NOT_FOUND;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED;
using google::scp::core::errors::
    SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND;
using google::scp::core::utils::Base64Decode;

namespace google::scp::cpio::client_providers {
namespace {

/// Filename for logging errors
constexpr std::string_view kTeeAwsKmsClientProvider = "TeeAwsKmsClientProvider";

std::vector<std::string> BuildDecryptArgs(std::string region,
                                          std::string ciphertext,
                                          std::string access_key_id,
                                          std::string access_key_secret,
                                          std::string security_token) noexcept {
  std::vector<std::string> args = {
      "decrypt",
      // Add `"--proxy-port 0"` for using the kmstool CLI inside an enclave with
      // proxy / proxify.
      "--proxy-port",
      "0",
  };
  if (!region.empty()) {
    args.push_back("--region");
    args.push_back(std::move(region));
  }
  if (!access_key_id.empty()) {
    args.push_back("--aws-access-key-id");
    args.push_back(std::move(access_key_id));
  }
  if (!access_key_secret.empty()) {
    args.push_back("--aws-secret-access-key");
    args.push_back(std::move(access_key_secret));
  }
  if (!security_token.empty()) {
    args.push_back("--aws-session-token");
    args.push_back(std::move(security_token));
  }
  if (!ciphertext.empty()) {
    args.push_back("--ciphertext");
    args.push_back(std::move(ciphertext));
  }
  return args;
}

}  // namespace

absl::Status TeeAwsKmsClientProvider::Decrypt(
    core::AsyncContext<DecryptRequest, DecryptResponse>&
        decrypt_context) noexcept {
  const auto& ciphertext = decrypt_context.request->ciphertext();
  if (ciphertext.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_CIPHER_TEXT_NOT_FOUND);
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result,
                      "Failed to get cipher text from decryption request.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  const auto& assume_role_arn = decrypt_context.request->account_identity();
  if (assume_role_arn.empty()) {
    auto execution_result = FailureExecutionResult(
        SC_TEE_AWS_KMS_CLIENT_PROVIDER_ASSUME_ROLE_NOT_FOUND);
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get AssumeRole Arn.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  const auto& kms_region = decrypt_context.request->kms_region();
  if (kms_region.empty()) {
    auto execution_result =
        FailureExecutionResult(SC_TEE_AWS_KMS_CLIENT_PROVIDER_REGION_NOT_FOUND);
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get region.");
    decrypt_context.Finish(execution_result);
    return absl::InvalidArgumentError(
        google::scp::core::errors::GetErrorMessage(
            execution_result.status_code));
  }

  auto get_credentials_request = std::make_shared<GetRoleCredentialsRequest>();
  get_credentials_request->account_identity =
      std::make_shared<AccountIdentity>(assume_role_arn);
  AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>
      get_session_credentials_context(
          std::move(get_credentials_request),
          absl::bind_front(
              &TeeAwsKmsClientProvider::GetSessionCredentialsCallbackToDecrypt,
              this, decrypt_context),
          decrypt_context);
  return credential_provider_->GetRoleCredentials(
      get_session_credentials_context);
}

void TeeAwsKmsClientProvider::GetSessionCredentialsCallbackToDecrypt(
    AsyncContext<DecryptRequest, DecryptResponse>& decrypt_context,
    AsyncContext<GetRoleCredentialsRequest, GetRoleCredentialsResponse>&
        get_session_credentials_context) noexcept {
  auto execution_result = get_session_credentials_context.result;
  if (!execution_result.Successful()) {
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context,
                      execution_result, "Failed to get AWS Credentials.");
    decrypt_context.Finish(execution_result);
    return;
  }

  const auto& get_session_credentials_response =
      *get_session_credentials_context.response;

  std::vector<std::string> decrypt_args =
      BuildDecryptArgs(decrypt_context.request->kms_region(),
                       decrypt_context.request->ciphertext(),
                       *get_session_credentials_response.access_key_id,
                       *get_session_credentials_response.access_key_secret,
                       *get_session_credentials_response.security_token);

  const ExecutionResultOr<std::string> plaintext =
      DecryptUsingEnclavesKmstoolCli(std::string(kAwsNitroEnclavesCliPath),
                                     std::move(decrypt_args));

  if (!plaintext.Successful()) {
    decrypt_context.Finish(plaintext.result());
    return;
  }

  // Decode the plaintext.
  std::string decoded_plaintext;
  ExecutionResult execute_result = Base64Decode(*plaintext, decoded_plaintext);
  if (!execute_result.Successful()) {
    SCP_ERROR_CONTEXT(kTeeAwsKmsClientProvider, decrypt_context, execute_result,
                      "Failed to decode data.");
    decrypt_context.Finish(execute_result);
    return;
  }

  auto kms_decrypt_response = std::make_shared<DecryptResponse>();
  kms_decrypt_response->set_plaintext(std::move(decoded_plaintext));
  decrypt_context.response = kms_decrypt_response;
  decrypt_context.Finish(SuccessExecutionResult());
}

ExecutionResultOr<std::string>
TeeAwsKmsClientProvider::DecryptUsingEnclavesKmstoolCli(
    std::string command, std::vector<std::string> args) noexcept {
  std::vector<char*> exec_args;
  exec_args.reserve(args.size() + 2);
  exec_args.push_back(command.data());
  for (std::string& arg : args) {
    exec_args.push_back(arg.data());
  }
  exec_args.push_back(nullptr);
  if (absl::StatusOr<std::string> output = utils::Exec(exec_args);
      !output.ok()) {
    core::FailureExecutionResult execution_result(
        google::scp::core::errors::
            SC_TEE_AWS_KMS_CLIENT_PROVIDER_KMSTOOL_CLI_EXECUTION_FAILED);
    SCP_ERROR(kTeeAwsKmsClientProvider, google::scp::core::common::kZeroUuid,
              execution_result, "%s", std::move(output).status().message());
    return execution_result;
  } else {
    std::string_view plaintext = *output;
    absl::ConsumePrefix(&plaintext, "PLAINTEXT: ");
    absl::ConsumeSuffix(&plaintext, "\n");
    return std::string(plaintext);
  }
}

std::unique_ptr<KmsClientProviderInterface> KmsClientProviderFactory::Create(
    absl::Nonnull<RoleCredentialsProviderInterface*> role_credentials_provider,
    core::AsyncExecutorInterface* /*io_async_executor*/) noexcept {
  return std::make_unique<TeeAwsKmsClientProvider>(role_credentials_provider);
}
}  // namespace google::scp::cpio::client_providers
