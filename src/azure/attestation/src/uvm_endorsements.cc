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

#ifndef ATTESTATION_UVM_ENDORSEMENTS_H
#define ATTESTATION_UVM_ENDORSEMENTS_H

#include "utils/security_context.h"

using google::scp::azure::attestation::utils::getSecurityContextFile;

namespace google::scp::azure::attestation {

std::string getSnpUvmEndorsements() {
  return getSecurityContextFile("/reference-info-base64");
}

}  // namespace google::scp::azure::attestation

#endif  // ATTESTATION_UVM_ENDORSEMENTS_H
