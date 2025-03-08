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

#include "utils/host_amd_certs.h"

using google::scp::azure::attestation::utils::getHostAmdCerts;

namespace google::scp::azure::attestation {

std::string getSnpEndorsedTcb() {
  auto host_certs_json = getHostAmdCerts();

  // Extract the endorsed TCB from the JSON
  std::string endorsed_tcb_reversed_endian = host_certs_json["tcbm"];

  // Reverse the endianess of the endorsed TCB
  std::string endorsed_tcb = "";
  for (int i = endorsed_tcb_reversed_endian.length() - 2; i >= 0; i -= 2) {
    endorsed_tcb += endorsed_tcb_reversed_endian.substr(i, 2);
  }

  return endorsed_tcb;
}

}  // namespace google::scp::azure::attestation
