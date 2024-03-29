// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "src/core/async_executor/async_executor.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/global_cpio/mock/mock_lib_cpio_provider_with_overrides.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::HttpClientInterface;
using google::scp::cpio::client_providers::AuthTokenProviderInterface;
using google::scp::cpio::client_providers::InstanceClientProviderInterface;
using google::scp::cpio::client_providers::RoleCredentialsProviderInterface;
using google::scp::cpio::client_providers::mock::
    MockLibCpioProviderWithOverrides;
using ::testing::IsNull;
using ::testing::NotNull;

namespace google::scp::cpio::test {
TEST(LibCpioProviderTest, AsyncExecutorCreated) {
  auto lib_cpio_provider = std::make_unique<MockLibCpioProviderWithOverrides>();
  ASSERT_SUCCESS(lib_cpio_provider->Init());
  ASSERT_SUCCESS(lib_cpio_provider->Run());
  lib_cpio_provider->GetCpuAsyncExecutor();
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, IOAsyncExecutorCreated) {
  auto lib_cpio_provider = std::make_unique<MockLibCpioProviderWithOverrides>();
  ASSERT_SUCCESS(lib_cpio_provider->Init());
  ASSERT_SUCCESS(lib_cpio_provider->Run());
  lib_cpio_provider->GetIoAsyncExecutor();
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, Http2ClientCreated) {
  auto lib_cpio_provider = std::make_unique<MockLibCpioProviderWithOverrides>();
  ASSERT_SUCCESS(lib_cpio_provider->Init());
  ASSERT_SUCCESS(lib_cpio_provider->Run());
  lib_cpio_provider->GetHttpClient();
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, Http1ClientCreated) {
  auto lib_cpio_provider = std::make_unique<MockLibCpioProviderWithOverrides>();
  ASSERT_SUCCESS(lib_cpio_provider->Init());
  ASSERT_SUCCESS(lib_cpio_provider->Run());
  lib_cpio_provider->GetHttp1Client();
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, RoleCredentialsProviderCreated) {
  auto lib_cpio_provider = std::make_unique<MockLibCpioProviderWithOverrides>();
  ASSERT_SUCCESS(lib_cpio_provider->Init());
  ASSERT_SUCCESS(lib_cpio_provider->Run());
  auto role_credentials_provider =
      lib_cpio_provider->GetRoleCredentialsProvider();
  ASSERT_TRUE(role_credentials_provider.ok());
  ASSERT_THAT(*role_credentials_provider, NotNull());
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}

TEST(LibCpioProviderTest, AuthTokenProviderCreated) {
  auto lib_cpio_provider = std::make_unique<MockLibCpioProviderWithOverrides>();
  ASSERT_SUCCESS(lib_cpio_provider->Init());
  ASSERT_SUCCESS(lib_cpio_provider->Run());
  lib_cpio_provider->GetAuthTokenProvider();
  EXPECT_SUCCESS(lib_cpio_provider->Stop());
}
}  // namespace google::scp::cpio::test
