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

#include <string>

#include "src/core/utils/base64.h"
#include "utils/host_amd_certs.h"

using google::scp::azure::attestation::utils::getHostAmdCerts;
using google::scp::core::utils::Base64Encode;

namespace google::scp::azure::attestation {

std::string replace(const std::string& input, const std::string toReplace,
                    const std::string replaceWith) {
  std::string output = input;
  size_t pos = 0;
  while ((pos = output.find(toReplace, pos)) != std::string::npos) {
    output.replace(pos, toReplace.length(), replaceWith);
    pos += replaceWith.length();
  }
  return output;
}

std::string getSnpEndorsements() {
  auto host_certs_json = getHostAmdCerts();

  // Extract the certs from the JSON
  std::string vcekCert = host_certs_json["vcekCert"].dump();
  std::string certificateChain = host_certs_json["certificateChain"].dump();

  // Combine the certs into a chain and cleanup characters that shouldn't be
  // there
  std::string endorsementCerts = vcekCert + certificateChain;
  endorsementCerts.erase(
      std::remove(endorsementCerts.begin(), endorsementCerts.end(), '\"'),
      endorsementCerts.cend());
  endorsementCerts = replace(endorsementCerts, "\\n", "\n");

  // Base64 encode the certificate chain
  Base64Encode(endorsementCerts, endorsementCerts);
  return endorsementCerts;
}

}  // namespace google::scp::azure::attestation
