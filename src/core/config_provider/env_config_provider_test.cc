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

#include "src/core/config_provider/env_config_provider.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <stdlib.h>

#include <filesystem>
#include <fstream>
#include <list>
#include <memory>

#include "src/core/config_provider/error_codes.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::EnvConfigProvider;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::StrEq;

namespace google::scp::core::test {

TEST(EnvConfigProviderTest, GetConfigsHappyPath) {
  EnvConfigProvider config;

  config.Init();

  std::string out_string;
  putenv(const_cast<char*>("key-for-string-value=10.10.10.20"));

  bool out_bool;
  putenv(const_cast<char*>("key-for-bool-value=true"));

  size_t out_size_t;
  putenv(const_cast<char*>("key-for-sizet-value=5000"));

  int32_t out_int32_t;
  putenv(const_cast<char*>("key-for-int32t-value=6000"));

  std::list<std::string> out_string_list;
  putenv(const_cast<char*>("key-for-string-list=1,2"));

  std::list<int32_t> out_int32_t_list;
  putenv(const_cast<char*>("key-for-int32t-list=1,2"));

  std::list<size_t> out_size_t_list;
  putenv(const_cast<char*>("key-for-sizet-list=3,4"));

  std::list<bool> out_bool_list;
  putenv(const_cast<char*>("key-for-bool-list=true,false"));

  // string
  EXPECT_SUCCESS(config.Get("key-for-string-value", out_string));
  // bool
  EXPECT_SUCCESS(config.Get("key-for-bool-value", out_bool));
  // size_t
  EXPECT_SUCCESS(config.Get("key-for-sizet-value", out_size_t));
  // int32_t
  EXPECT_SUCCESS(config.Get("key-for-int32t-value", out_int32_t));
  // string list
  EXPECT_SUCCESS(config.Get("key-for-string-list", out_string_list));
  // int32_t list
  EXPECT_SUCCESS(config.Get("key-for-int32t-list", out_int32_t_list));
  // size_t list
  EXPECT_SUCCESS(config.Get("key-for-sizet-list", out_size_t_list));
  // bool list
  EXPECT_SUCCESS(config.Get("key-for-bool-list", out_bool_list));

  EXPECT_THAT(out_string, StrEq("10.10.10.20"));
  EXPECT_THAT(out_size_t, Eq(5000));
  EXPECT_THAT(out_int32_t, Eq(6000));
  EXPECT_TRUE(out_bool);
  EXPECT_THAT(out_string_list, ElementsAre(StrEq("1"), StrEq("2")));
  EXPECT_THAT(out_int32_t_list, ElementsAre(1, 2));
  EXPECT_THAT(out_size_t_list, ElementsAre(3, 4));
  EXPECT_THAT(out_bool_list, ElementsAre(IsTrue(), IsFalse()));
}

TEST(EnvConfigProviderTest, WhenSetToEmtpyValueGetStringValueShouldSucceed) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("a-var-thats-empty="));

  std::string out_string;
  EXPECT_SUCCESS(config.Get("a-var-thats-empty", out_string));
  EXPECT_THAT(out_string, IsEmpty());
}

TEST(EnvConfigProviderTest, WhenSetToEmptyValueGetNonStringValueShouldFail) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("another-var-thats-empty="));

  int32_t out_int32;
  EXPECT_THAT(config.Get("another-var-thats-empty", out_int32),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetStringFailsWhenKeyDoesNotExist) {
  EnvConfigProvider config;
  config.Init();

  std::string out_string;
  EXPECT_THAT(config.Get("non-existing-key", out_string),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));
}

TEST(EnvConfigProviderTest, GetInt32TFailsWhenValueIsNotInt32) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("non-int32-val=hello"));

  int32_t out_val;
  EXPECT_THAT(config.Get("non-int32-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetSizeTFailsWhenValueIsNotSizeT) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("non-size-t-val=hello"));

  size_t out_val;
  EXPECT_THAT(config.Get("non-size-t-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetBoolFailsWhenValueIsNotBool) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("non-bool-val=hello"));

  bool out_val;
  EXPECT_THAT(config.Get("non-bool-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetStringListFailsWhenDoesNotExist) {
  EnvConfigProvider config;
  config.Init();

  std::list<std::string> out_val;
  EXPECT_THAT(config.Get("non-existing-val", out_val),
              ResultIs(FailureExecutionResult(
                  errors::SC_CONFIG_PROVIDER_KEY_NOT_FOUND)));
}

TEST(EnvConfigProviderTest, GetStringListShouldHandleSingleItem) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("single-item-list=1"));

  std::list<std::string> out_val;
  auto ret = config.Get("single-item-list", out_val);

  EXPECT_SUCCESS(ret);
  std::list<std::string> expected_list({"1"});
  EXPECT_EQ(out_val, expected_list);
}

TEST(EnvConfigProviderTest, GetInt32TListShouldFailWhenNotInt32TList) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("not-int32t-list=a,2,c"));

  std::list<int32_t> out_val;
  auto ret = config.Get("not-int32t-list", out_val);

  EXPECT_THAT(ret, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetSizeTListShouldFailWhenNotSizeTList) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("not-sizet-list=a,2,c"));

  std::list<size_t> out_val;
  auto ret = config.Get("not-sizet-list", out_val);

  EXPECT_THAT(ret, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}

TEST(EnvConfigProviderTest, GetBoolListShouldFailWhenNotBoolList) {
  EnvConfigProvider config;
  config.Init();

  putenv(const_cast<char*>("not-bool-list=a,true,c"));

  std::list<bool> out_val;
  auto ret = config.Get("not-bool-list", out_val);

  EXPECT_THAT(ret, ResultIs(FailureExecutionResult(
                       errors::SC_CONFIG_PROVIDER_VALUE_TYPE_ERROR)));
}
}  // namespace google::scp::core::test
