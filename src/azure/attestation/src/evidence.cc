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

#include <fcntl.h>

#include <openssl/base64.h>

#include "absl/log/check.h"
#include "src/core/utils/base64.h"

#include "attestation.h"
#include "sev5.h"
#include "sev6.h"

namespace google::scp::azure::attestation {

std::string base64EncodeBytes(const uint8_t* decoded, size_t size) {
  size_t required_len = 0;
  EVP_EncodedLength(&required_len, size);
  auto buffer = std::make_unique<uint8_t[]>(required_len);
  int ret = EVP_EncodeBlock(buffer.get(), decoded, size);
  return std::string(reinterpret_cast<char*>(buffer.get()), ret);
}

std::string getSnpEvidence(const std::string report_data) {
  std::unique_ptr<SnpReport> report;

  switch (getSnpType()) {
    case SnpType::SEV:
      std::cout << "Getting report from /dev/sev" << std::endl;
      report = sev5::getReport(report_data);
      break;
    case SnpType::SEV_GUEST:
      std::cout << "Getting report from /dev/sev-guest" << std::endl;
      report = sev6::getReport(report_data);
      break;
    default:
      CHECK(false) << "Unsupported or no SNP type";
  }

  return base64EncodeBytes(reinterpret_cast<uint8_t*>(report.get()),
                           sizeof(SnpReport));
}

}  // namespace google::scp::azure::attestation
