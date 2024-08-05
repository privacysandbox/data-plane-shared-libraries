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

#ifndef CORE_LOGGER_MOCK_MOCK_LOG_PROVIDER_H_
#define CORE_LOGGER_MOCK_MOCK_LOG_PROVIDER_H_

#include <cstdarg>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "src/core/common/uuid/uuid.h"
#include "src/core/logger/interface/log_provider_interface.h"
#include "src/core/logger/log_providers/console_log_provider.h"
#include "src/core/logger/log_utils.h"

namespace google::scp::core::logger::mock {
class MockLogProvider final : public ConsoleLogProvider {
 public:
  void Print(std::string_view output) noexcept override {
    messages_.emplace_back(output);
  }

  std::vector<std::string> GetMessages() const { return messages_; }

  void Clear() { return messages_.clear(); }

 private:
  std::vector<std::string> messages_;
};
}  // namespace google::scp::core::logger::mock

#endif  // CORE_LOGGER_MOCK_MOCK_LOG_PROVIDER_H_
