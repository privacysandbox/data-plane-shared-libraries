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

#include "test_aws_instance_client_provider.h"

#include <string>
#include <string_view>

#include "absl/strings/substitute.h"

namespace {
constexpr std::string_view kAwsResourceNameFormat =
    R"(arn:aws:ec2:$0:$1:instance/$2)";
}  // namespace

namespace google::scp::cpio::client_providers {

absl::Status TestAwsInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  resource_name =
      absl::Substitute(kAwsResourceNameFormat, test_options_.region,
                       test_options_.project_id, test_options_.instance_id);
  return absl::OkStatus();
}

}  // namespace google::scp::cpio::client_providers
