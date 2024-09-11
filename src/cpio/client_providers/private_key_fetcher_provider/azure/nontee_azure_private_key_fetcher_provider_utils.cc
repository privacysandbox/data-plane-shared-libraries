/*
 * Portions Copyright (c) Microsoft Corporation
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

#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "src/azure/attestation/src/attestation.h"

#include "azure_private_key_fetcher_provider_utils.h"

using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::Uri;
using google::scp::cpio::client_providers::AzurePrivateKeyFetchingClientUtils;

namespace {
constexpr char kAttestation[] = "attestation";
}

namespace google::scp::cpio::client_providers {
void AzurePrivateKeyFetchingClientUtils::CreateHttpRequest(
    const PrivateKeyFetchingRequest& request, HttpRequest& http_request) {
  const auto& base_uri =
      request.key_vending_endpoint->private_key_vending_service_endpoint;
  http_request.method = HttpMethod::POST;

  http_request.path = std::make_shared<Uri>(base_uri);

  // Get Attestation Report
  CHECK(!hasSnp()) << "It's in a SNP environment";
  const auto report = fetchFakeSnpAttestation();
  CHECK(report.has_value()) << "Failed to get attestation report";

  nlohmann::json json_obj;
  json_obj[kAttestation] = report.value();

  http_request.body = core::BytesBuffer(json_obj.dump());
}

}  // namespace google::scp::cpio::client_providers
