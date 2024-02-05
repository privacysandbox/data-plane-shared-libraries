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

#ifndef JSON_ATTESTATION_REPORT_H
#define JSON_ATTESTATION_REPORT_H

#include <string>

#include <nlohmann/json.hpp>

#include "get-snp-report/get-snp-report.h"

#include "security_context_fetcher.h"

bool hasSnp();
nlohmann::json fetchFakeSnpAttestation();
nlohmann::json fetchSnpAttestation(const std::string report_data = "");

extern "C" {
bool fetchSnpReport(const char* report_data_hexstring, void* snp_report);
}
#endif  // JSON_ATTESTATION_REPORT_H
