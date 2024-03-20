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

#include "aws_instance_client_utils.h"

#include <memory>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "src/core/common/uuid/uuid.h"
#include "src/core/interface/http_types.h"
#include "src/public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;
using google::scp::core::errors::
    SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME;

namespace {
constexpr std::string_view kAwsInstanceClientUtils = "AwsInstanceClientUtils";

// Aws resource name format:
//     arn:partition:service:region:account-id:resource-id
//     arn:partition:service:region:account-id:resource-type/resource-id
//     arn:partition:service:region:account-id:resource-type:resource-id
//
// For some resources, region and account-id can be empty.
// For more information, refers to
// https://docs.aws.amazon.com/general/latest/gr/aws-arns-and-namespaces.html
constexpr std::string_view kResourceNameRegex =
    R"(arn:(aws|aws-cn|aws-us-gov):[a-z0-9]+:([a-z0-9-]*):(\d*):(.*))";
}  // namespace

namespace google::scp::cpio::client_providers {
ExecutionResultOr<std::string> AwsInstanceClientUtils::GetCurrentRegionCode(
    InstanceClientProviderInterface& instance_client) noexcept {
  std::string instance_resource_name;
  if (absl::Status error = instance_client.GetCurrentInstanceResourceNameSync(
          instance_resource_name);
      !error.ok()) {
    SCP_ERROR(kAwsInstanceClientUtils, kZeroUuid, error,
              "Failed getting instance resource name.");
    return FailureExecutionResult(SC_UNKNOWN);
  }

  auto region_code_or = ParseRegionFromResourceName(instance_resource_name);
  if (!region_code_or.Successful()) {
    SCP_ERROR(
        kAwsInstanceClientUtils, kZeroUuid, region_code_or.result(),
        "Failed to parse instance resource name %s to get aws region code",
        instance_resource_name.c_str());
  }

  return std::move(*region_code_or);
}

ExecutionResultOr<std::string>
AwsInstanceClientUtils::ParseRegionFromResourceName(
    std::string_view resource_name) noexcept {
  AwsResourceNameDetails details;
  auto result = GetResourceNameDetails(resource_name, details);
  RETURN_IF_FAILURE(result);
  return details.region;
}

ExecutionResultOr<std::string>
AwsInstanceClientUtils::ParseAccountIdFromResourceName(
    std::string_view resource_name) noexcept {
  AwsResourceNameDetails details;
  auto result = GetResourceNameDetails(resource_name, details);
  RETURN_IF_FAILURE(result);
  return details.account_id;
}

ExecutionResultOr<std::string>
AwsInstanceClientUtils::ParseInstanceIdFromInstanceResourceName(
    std::string_view resource_name) noexcept {
  AwsResourceNameDetails details;
  auto result = GetResourceNameDetails(resource_name, details);
  RETURN_IF_FAILURE(result);
  return details.resource_id;
}

ExecutionResult AwsInstanceClientUtils::ValidateResourceNameFormat(
    std::string_view resource_name) noexcept {
  static std::regex re(std::string{kResourceNameRegex});
  if (!std::regex_match(std::string{resource_name}, re)) {
    return FailureExecutionResult(
        SC_AWS_INSTANCE_CLIENT_INVALID_INSTANCE_RESOURCE_NAME);
  }

  return SuccessExecutionResult();
}

ExecutionResult AwsInstanceClientUtils::GetResourceNameDetails(
    std::string_view resource_name, AwsResourceNameDetails& detail) noexcept {
  auto result = ValidateResourceNameFormat(resource_name);
  RETURN_IF_FAILURE(result);

  std::vector<std::string> splits = absl::StrSplit(resource_name, ":");
  detail.account_id = splits[4];
  detail.region = splits[3];

  // remove prefix path from resource id.
  std::vector<std::string> id_splits = absl::StrSplit(splits.back(), "/");
  detail.resource_id = id_splits.back();

  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio::client_providers
