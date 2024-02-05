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

#include "get-snp-report.h"

bool fetchSnpReport(const char* report_data_hexstring, void** snp_report) {
  bool success = false;
  uint8_t* snp_report_hex;

  if (supportsDevSev()) {
    success =
        fetchAttestationReport5(report_data_hexstring, (void*)&snp_report_hex);
  } else if (supportsDevSevGuest()) {
    success =
        fetchAttestationReport6(report_data_hexstring, (void*)&snp_report_hex);
  } else {
    fprintf(stderr, "No supported SNP device found\n");
  }

  if (success) {
    *snp_report = snp_report_hex;
    return 0;
  }

  return -1;
}
