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

#include "sev6.h"

#include <fcntl.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sys/types.h>

#include <algorithm>
#include <memory>
#include <string>

#include "absl/log/check.h"
#include "absl/strings/escaping.h"

namespace google::scp::azure::attestation::sev6 {

namespace {
/* linux kernel 6.* versions of the ioctls that talk to the PSP */

// aka/replaced by this from include/uapi/linux/sev-guest.h
//
struct Request {
  uint8_t msg_version;  // message version number (must be non-zero)
  uint64_t req_data;    // Request and response structure address
  uint64_t resp_data;
  uint64_t fw_err;  // firmware error code on failure (see psp-sev.h)
};

struct RequestWrapper {
  /* response data, see SEV-SNP spec for the format */
  uint8_t data[4000];
};

#define SNP_GUEST_REQ_IOC_TYPE 'S'
#define SNP_GET_REPORT _IOWR(SNP_GUEST_REQ_IOC_TYPE, 0x0, struct Request)
#define SNP_GET_DERIVED_KEY _IOWR(SNP_GUEST_REQ_IOC_TYPE, 0x1, struct Request)
#define SNP_GET_EXT_REPORT _IOWR(SNP_GUEST_REQ_IOC_TYPE, 0x2, struct Request)

}  // namespace

std::unique_ptr<SnpReport> getReport(const std::string report_data) {
  SnpRequest request = {};
  auto decoded_bytes = absl::HexStringToBytes(report_data);
  size_t num_bytes_to_copy =
      std::min(decoded_bytes.size(), sizeof(request.report_data));
  std::copy(decoded_bytes.begin(), decoded_bytes.begin() + num_bytes_to_copy,
            request.report_data);

  RequestWrapper resp_wrapper = {};

  Request payload = {
      .msg_version = 1,
      .req_data = (uint64_t)&request,
      .resp_data = (uint64_t)&resp_wrapper,
  };

  auto sev_guest_file = open("/dev/sev-guest", O_RDWR | O_CLOEXEC);

  auto rc = ioctl(sev_guest_file, SNP_GET_REPORT, &payload);
  CHECK(rc >= 0) << "Failed to issue ioctl SNP_GET_REPORT";

  SnpResponse* response = (SnpResponse*)&resp_wrapper.data;

  auto report = std::make_unique<SnpReport>();
  *report = response->report;
  return report;
}

}  // namespace google::scp::azure::attestation::sev6
