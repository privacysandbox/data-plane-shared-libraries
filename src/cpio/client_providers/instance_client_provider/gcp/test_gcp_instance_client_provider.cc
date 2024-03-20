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

#include "test_gcp_instance_client_provider.h"

#include <string>
#include <string_view>

#include "absl/strings/substitute.h"

namespace {
constexpr std::string_view kGcpResourceNameFormat =
    R"(//compute.googleapis.com/projects/$0/zones/$1/instances/$2)";
}  // namespace

namespace google::scp::cpio::client_providers {

absl::Status TestGcpInstanceClientProvider::GetCurrentInstanceResourceNameSync(
    std::string& resource_name) noexcept {
  resource_name =
      absl::Substitute(kGcpResourceNameFormat, test_options_.project_id,
                       test_options_.zone, test_options_.instance_id);
  return absl::OkStatus();
}

}  // namespace google::scp::cpio::client_providers
