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

#include "src/core/config_provider/config_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <list>
#include <memory>
#include <utility>

#include "src/core/config_provider/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::ConfigProvider;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::StrEq;

namespace google::scp::core::test {
std::filesystem::path GetTestDataDir(std::string relative_path) {
  std::filesystem::path test_srcdir_env = std::getenv("TEST_SRCDIR");
  std::filesystem::path test_workspace_env = std::getenv("TEST_WORKSPACE");

  return test_srcdir_env / test_workspace_env / std::move(relative_path);
}

TEST(ConfigProviderTest, GetConfigs) {
  std::filesystem::path relative_path =
      "src/core/config_provider/resources/test_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);

  ConfigProvider config(full_path);

  config.Init();

  std::string out_string;
  size_t out_int;
  bool out_bool;
  std::list<std::string> out_string_list;
  std::list<int32_t> out_int_list;
  std::list<size_t> out_size_list;
  std::list<bool> out_bool_list;

  config.Get("server-ip", out_string);
  config.Get("server-run", out_bool);
  config.Get("buffer-length", out_int);
  config.Get("string-list", out_string_list);
  config.Get("int-list", out_int_list);
  config.Get("size-list", out_size_list);
  config.Get("bool-list", out_bool_list);

  EXPECT_THAT(out_string, StrEq("10.10.10.20"));
  EXPECT_THAT(out_int, Eq(5000));
  EXPECT_THAT(out_bool, IsTrue());
  EXPECT_THAT(out_string_list, ElementsAre(StrEq("1"), StrEq("2")));
  EXPECT_THAT(out_int_list, ElementsAre(1, 2));
  EXPECT_THAT(out_size_list, ElementsAre(3, 4));
  EXPECT_THAT(out_bool_list, ElementsAre(IsTrue(), IsFalse()));
}

TEST(ConfigProviderTest, GetConfigsFailed) {
  std::filesystem::path relative_path =
      "src/core/config_provider/resources/test_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);
  ConfigProvider config(full_path);
  config.Init();
  std::string out_string;
  ASSERT_SUCCESS(config.Init());
  EXPECT_THAT(config.Get("server-name", out_string),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));
  EXPECT_THAT(config.Get("buffer-length", out_string),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(ConfigProviderTest, InitFailed) {
  std::filesystem::path relative_path =
      "src/core/config_provider/resources/unknown_config.json";
  std::filesystem::path full_path = GetTestDataDir(relative_path);
  ConfigProvider config(full_path);
  const auto res = config.Init();
  EXPECT_THAT(res, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_CANNOT_PARSE_CONFIG_FILE)));
  EXPECT_STREQ(errors::GetErrorMessage(res.status_code).data(),
               "Config provider cannot load config file");
}

}  // namespace google::scp::core::test
