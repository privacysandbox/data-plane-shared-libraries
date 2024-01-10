/*
 * Copyright 2022 Google LLC
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

#ifndef ROMA_CONFIG_SRC_CONFIG_LOG_NONPROD_H_
#define ROMA_CONFIG_SRC_CONFIG_LOG_NONPROD_H_

#include <string_view>

#include "absl/log/log.h"

namespace google::scp::roma {

template <typename TMetadata>
void LogFunction(absl::LogSeverity severity, TMetadata metadata,
                 std::string_view msg) {
  LOG(LEVEL(severity)) << msg;
  return;
}

}  // namespace google::scp::roma

#endif  // ROMA_CONFIG_SRC_CONFIG_LOG_NONPROD_H_
