/*
 * Copyright 2023 Google LLC
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

#include "logging.h"

#include <cstdlib>
#include <limits>

#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "src/roma/config/config.h"

namespace google::scp::roma::logging {

int GetVlogVerboseLevel() {
  static const int external_verbose_level = [] {
    int lvl = std::numeric_limits<int>::min();
    const char* env_var = std::getenv(kRomaVlogLevel.data());
    if (env_var == nullptr) {
      return lvl;
    }
    CHECK(absl::SimpleAtoi(env_var, &lvl) && lvl >= 0)
        << "ROMA_VLOG_LEVEL needs to be an integer >= 0";
    return lvl;
  }();
  return external_verbose_level;
}

bool VLogIsOn(int verbose_level) {
  return verbose_level <= GetVlogVerboseLevel();
}
}  // namespace google::scp::roma::logging
