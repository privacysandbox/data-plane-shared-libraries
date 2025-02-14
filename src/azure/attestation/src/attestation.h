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

#ifndef AZURE_ATTESTATION_H
#define AZURE_ATTESTATION_H

#include <fstream>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace google::scp::azure::attestation {

/* from SEV-SNP Firmware ABI Specification Table 20 */
struct SnpRequest {
  uint8_t report_data[64];
  uint32_t vmpl;
  uint8_t reserved[28];  // needs to be zero
};

enum SNP_MSG_TYPE {
  SNP_MSG_TYPE_INVALID = 0,
  SNP_MSG_CPUID_REQ,
  SNP_MSG_CPUID_RSP,
  SNP_MSG_KEY_REQ,
  SNP_MSG_KEY_RSP,
  SNP_MSG_REPORT_REQ,
  SNP_MSG_REPORT_RSP,
  SNP_MSG_EXPORT_REQ,
  SNP_MSG_EXPORT_RSP,
  SNP_MSG_IMPORT_REQ,
  SNP_MSG_IMPORT_RSP,
  SNP_MSG_ABSORB_REQ,
  SNP_MSG_ABSORB_RSP,
  SNP_MSG_VMRK_REQ,
  SNP_MSG_VMRK_RSP,
  SNP_MSG_TYPE_MAX
};

/* from SEV-SNP Firmware ABI Specification from Table 21 */
struct SnpReport {
  uint32_t version;       // version no. of this attestation report.
                          // Set to 1 for this specification.
  uint32_t guest_svn;     // The guest SVN
  uint64_t policy;        // see table 8 - various settings
  __uint128_t family_id;  // as provided at launch
  __uint128_t image_id;   // as provided at launch
  uint32_t vmpl;          // the request VMPL for the attestation
                          // report
  uint32_t signature_algo;
  uint64_t platform_version;      // The install version of the firmware
  uint64_t platform_info;         // information about the platform see table
                                  // 22
                                  // not going to try to use bit fields for
                                  // this next one. Too confusing as to which
                                  // bit of the byte will be used. Make a mask
                                  // if you need it
  uint32_t author_key_en;         // 31 bits of reserved, must be zero, bottom
                                  // bit indicates that the digest of the
                                  // author key is present in
                                  // AUTHOR_KEY_DIGEST. Set to the value of
                                  // GCTX.AuthorKeyEn.
  uint32_t reserved1;             // must be zero
  uint8_t report_data[64];        // Guest provided data.
  uint8_t measurement[48];        // measurement calculated at launch
  uint8_t host_data[32];          // data provided by the hypervisor at launch
  uint8_t id_key_digest[48];      // SHA-384 digest of the ID public key that
                                  // signed the ID block provided in
                                  // SNP_LAUNCH_FINISH
  uint8_t author_key_digest[48];  // SHA-384 digest of the Author public key
                                  // that certified the ID key, if provided in
                                  // SNP_LAUNCH_FINISH. Zeros if author_key_en
                                  // is 1 (sounds backwards to me).
  uint8_t report_id[32];          // Report ID of this guest.
  uint8_t report_id_ma[32];       // Report ID of this guest's mmigration
                                  // agent.
  uint64_t reported_tcb;          // Reported TCB version used to derive the
                                  // VCEK that signed this report
  uint8_t reserved2[24];          // reserved
  uint8_t chip_id[64];            // Identifier unique to the chip
  uint8_t committed_svn[8];       // The current commited SVN of the firware
                                  // (version 2 report feature)
  uint8_t committed_version[8];   // The current commited version of the
                                  // firware
  uint8_t launch_svn[8];          // The SVN that this guest was launched or
                                  // migrated at
  uint8_t reserved3[168];         // reserved
  uint8_t signature[512];         // Signature of this attestation report.
                                  // See table 23.
};

/* from SEV-SNP Firmware ABI Specification Table 22 */
struct SnpResponse {
  uint32_t status;
  uint32_t report_size;
  uint8_t reserved[24];
  SnpReport report;
  uint8_t padding[64];  // padding to the size of SEV_SNP_REPORT_RSP_BUF_SZ
                        // (i.e., 1280 bytes)
};

enum SnpType { SEV, SEV_GUEST, NONE };

struct AttestationReport {
  std::string evidence;
  std::string endorsements;
  std::string uvm_endorsements;
  std::string endorsed_tcb;

  operator nlohmann::json() const {
    return nlohmann::json{{"evidence", evidence},
                          {"endorsements", endorsements},
                          {"uvm_endorsements", uvm_endorsements},
                          {"endorsed_tcb", endorsed_tcb}};
  }
};

SnpType getSnpType();

bool hasSnp();

std::optional<AttestationReport> fetchSnpAttestation(
    const std::string report_data = "");

std::optional<AttestationReport> fetchFakeSnpAttestation();

std::string getSnpEvidence(const std::string report_data);

std::string getSnpEndorsements();

std::string getSnpUvmEndorsements();

std::string getSnpEndorsedTcb();

}  // namespace google::scp::azure::attestation

#endif  // AZURE_ATTESTATION_H
