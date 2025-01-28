/*
 * Copyright 2024 Google LLC
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

#include <gtest/gtest.h>

#include "absl/base/log_severity.h"
#include "absl/log/log.h"
#include "absl/log/scoped_mock_log.h"
#include "absl/status/status.h"
#include "absl/synchronization/notification.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

using google::scp::roma::sandbox::roma_service::RomaService;
using ::testing::_;

namespace google::scp::roma::test {
void LogMetadataFunction(absl::LogSeverity severity,
                         absl::flat_hash_map<std::string, std::string> metadata,
                         std::string_view msg) {
  for (const auto& [key, value] : metadata) {
    LOG(LEVEL(severity)) << key << ": " << value;
  }
  LOG(LEVEL(severity)) << msg;
}
}  // namespace google::scp::roma::test
