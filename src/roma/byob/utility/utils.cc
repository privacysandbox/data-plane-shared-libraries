// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/byob/utility/utils.h"

#include <sys/capability.h>

#include "absl/log/log.h"
#include "src/roma/byob/interface/roma_service.h"

namespace privacy_sandbox::server_common::byob {

using ::privacy_sandbox::server_common::byob::Mode;

bool HasClonePermissionsByobWorker(Mode mode) {
  // For sandbox mode, since runsc container tries to clone workers, it doesn't
  // matter if the calling process had CAP_SYS_ADMIN.
  if (mode == Mode::kModeSandbox) {
    return true;
  }
  cap_t capabilities = cap_get_proc();
  cap_flag_value_t value;
  int result = cap_get_flag(capabilities, CAP_SYS_ADMIN, CAP_EFFECTIVE, &value);
  cap_free(capabilities);
  // If cap_get_flag fails, return true and allow the code to run and crash
  // loudly (as needed).
  if (result != 0) {
    PLOG(ERROR) << "Failed to cap_get_flag";
    return true;
  }
  if (value == CAP_SET) {
    LOG(INFO) << "Process has CAP_SYS_ADMIN";
    return true;
  } else {
    LOG(INFO) << "Process does not have CAP_SYS_ADMIN";
    return false;
  }
}

}  // namespace privacy_sandbox::server_common::byob
