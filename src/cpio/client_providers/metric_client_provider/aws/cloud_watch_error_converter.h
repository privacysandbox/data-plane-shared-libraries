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

#ifndef CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_AWS_CLOUD_WATCH_ERROR_CONVERTER_H_
#define CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_AWS_CLOUD_WATCH_ERROR_CONVERTER_H_

#include <string>

#include <aws/monitoring/CloudWatchClient.h>
#include <aws/monitoring/CloudWatchErrors.h>

#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class CloudWatchErrorConverter {
 public:
  /**
   * @brief Converts AWS CloudWatchErrors to CPIO AwsMetricClientProvider
   * errors.
   *
   * @param cloud_watch_error CloudWatchErrors enum.
   * @return core::ExecutionResult CPIO ExecutionResult with corresponding error
   * code.
   */
  static core::ExecutionResult ConvertCloudWatchError(
      Aws::CloudWatch::CloudWatchErrors cloud_watch_error,
      std::string_view error_message);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_METRIC_CLIENT_PROVIDER_AWS_CLOUD_WATCH_ERROR_CONVERTER_H_
