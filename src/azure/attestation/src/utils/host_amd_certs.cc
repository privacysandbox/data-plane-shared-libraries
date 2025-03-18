/*
 * Portions Copyright (c) Microsoft Corporation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "host_amd_certs.h"

using google::scp::azure::attestation::utils::getSecurityContextFile;
using google::scp::core::utils::Base64Decode;

namespace google::scp::azure::attestation::utils {

nlohmann::json getHostAmdCerts() {
  // Read the local Base64 encoded AMD certs
  const auto host_certs_b64 = getSecurityContextFile("/host-amd-cert-base64");

  // Decode the contents of the file
  std::string host_certs_str;
  Base64Decode(host_certs_b64, host_certs_str);

  // Parse the decoded string into JSON
  return nlohmann::json::parse(host_certs_str);
}

}  // namespace google::scp::azure::attestation::utils
