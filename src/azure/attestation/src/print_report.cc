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

#include <iostream>

#include "absl/log/check.h"

#include "attestation.h"

using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;

int main(int argc, char* argv[]) {
  std::string report_data = argc > 1 ? argv[1] : "";
  const auto report =
      hasSnp() ? fetchSnpAttestation(report_data) : fetchFakeSnpAttestation();

  CHECK(report.has_value()) << "Failed to get attestation report";
  std::cout << "report (fake=" << !hasSnp() << "):\n";
  std::cout << nlohmann::json(report.value()).dump(2) << std::endl;
  return 0;
}
