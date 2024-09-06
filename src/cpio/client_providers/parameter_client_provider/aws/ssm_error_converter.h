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

#ifndef CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AWS_SSM_ERROR_CONVERTER_H_
#define CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AWS_SSM_ERROR_CONVERTER_H_

#include <string>

#include <aws/ssm/SSMClient.h>

#include "src/cpio/client_providers/parameter_client_provider/aws/error_codes.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio::client_providers {
class SSMErrorConverter {
 public:
  /**
   * @brief Converts SSM Error to ExecutionResult with failure status
   *
   * @param error The SSM error comes from AWS.
   *
   * @return core::FailureExecutionResult The converted result of the operation.
   */
  static core::FailureExecutionResult ConvertSSMError(
      const Aws::SSM::SSMErrors& error, std::string_view error_message);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_AWS_SSM_ERROR_CONVERTER_H_
