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

#ifndef CORE_CONFIG_PROVIDER_MOCK_MOCK_CONFIG_PROVIDER_H_
#define CORE_CONFIG_PROVIDER_MOCK_MOCK_CONFIG_PROVIDER_H_

#include <list>
#include <string>
#include <utility>

#include "absl/container/flat_hash_map.h"
#include "src/core/config_provider/error_codes.h"
#include "src/core/interface/config_provider_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core::config_provider::mock {
class MockConfigProvider : public ConfigProviderInterface {
 public:
  MockConfigProvider() {}

  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); }

  ExecutionResult Get(const ConfigKey& key,
                      std::string& out) noexcept override {
    if (const auto it = string_config_map_.find(key);
        it != string_config_map_.end()) {
      out = it->second;
      return SuccessExecutionResult();
    } else {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
  }

  ExecutionResult Get(const ConfigKey& key, size_t& out) noexcept override {
    if (const auto it = size_t_config_map_.find(key);
        it != size_t_config_map_.end()) {
      out = it->second;
      return SuccessExecutionResult();
    } else {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
  }

  ExecutionResult Get(const ConfigKey& key, int32_t& out) noexcept override {
    if (const auto it = int32_t_config_map_.find(key);
        it != int32_t_config_map_.end()) {
      out = it->second;
      return SuccessExecutionResult();
    } else {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
  }

  ExecutionResult Get(const ConfigKey& key, bool& out) noexcept override {
    if (const auto it = bool_config_map_.find(key);
        it != bool_config_map_.end()) {
      out = it->second;
      return SuccessExecutionResult();
    } else {
      return FailureExecutionResult(errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND);
    }
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<std::string>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<int32_t>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<size_t>& out) noexcept override {
    return SuccessExecutionResult();
  }

  ExecutionResult Get(const ConfigKey& key,
                      std::list<bool>& out) noexcept override {
    return SuccessExecutionResult();
  }

  void Set(const ConfigKey& key, const char* value) {
    string_config_map_[key] = std::string(value);
  }

  void Set(const ConfigKey& key, std::string value) {
    string_config_map_[key] = std::move(value);
  }

  void SetInt(const ConfigKey& key, const size_t value) {
    size_t_config_map_[key] = value;
  }

  void SetBool(const ConfigKey& key, const bool value) {
    bool_config_map_[key] = value;
  }

 private:
  absl::flat_hash_map<ConfigKey, std::string> string_config_map_;
  absl::flat_hash_map<ConfigKey, size_t> size_t_config_map_;
  absl::flat_hash_map<ConfigKey, int32_t> int32_t_config_map_;
  absl::flat_hash_map<ConfigKey, bool> bool_config_map_;
};
}  // namespace google::scp::core::config_provider::mock

#endif  // CORE_CONFIG_PROVIDER_MOCK_MOCK_CONFIG_PROVIDER_H_
