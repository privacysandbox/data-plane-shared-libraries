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

#include "google/protobuf/any.pb.h"
#include "scp/cc/core/async_executor/src/async_executor.h"
#include "scp/cc/core/async_executor/src/error_codes.h"
#include "scp/cc/core/common/global_logger/src/global_logger.h"
#include "scp/cc/core/interface/async_executor_interface.h"
#include "scp/cc/core/message_router/src/message_router.h"
#include "scp/cc/cpio/client_providers/global_cpio/src/global_cpio.h"
#include "scp/cc/public/core/interface/execution_result.h"
#include "scp/cc/public/core/test/interface/execution_result_matchers.h"
#include "scp/cc/public/cpio/interface/cpio.h"
#include "scp/cc/public/cpio/interface/metric_client/metric_client_interface.h"
#include "scp/cc/public/cpio/interface/metric_client/type_def.h"
#include "scp/cc/public/cpio/test/global_cpio/test_cpio_options.h"
#include "scp/cc/public/cpio/test/global_cpio/test_lib_cpio.h"

using google::scp::core::AsyncExecutor;
using google::scp::core::AsyncExecutorInterface;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ASYNC_EXECUTOR_NOT_RUNNING;
using google::scp::core::test::ResultIs;
using google::scp::cpio::MetricClientFactory;
using google::scp::cpio::MetricClientInterface;
using google::scp::cpio::MetricClientOptions;
using google::scp::cpio::client_providers::GlobalCpio;
using ::testing::NotNull;

namespace google::scp::cpio::test {
namespace {
constexpr std::string_view kRegion = "us-east-1";

TEST(LibCpioTest, NoLogTest) {
  TestCpioOptions options;
  options.log_option = LogOption::kNoLog;
  options.region = kRegion;
  ASSERT_SUCCESS(TestLibCpio::InitCpio(options));
  EXPECT_THAT(GlobalCpio::GetGlobalCpio(), NotNull());
  EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(options));
}

TEST(LibCpioTest, ConsoleLogTest) {
  TestCpioOptions options;
  options.log_option = LogOption::kConsoleLog;
  options.region = kRegion;
  ASSERT_SUCCESS(TestLibCpio::InitCpio(options));
  EXPECT_THAT(GlobalCpio::GetGlobalCpio(), NotNull());
  EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(options));
}

TEST(LibCpioTest, SysLogTest) {
  TestCpioOptions options;
  options.log_option = LogOption::kSysLog;
  options.region = kRegion;
  ASSERT_SUCCESS(TestLibCpio::InitCpio(options));
  EXPECT_THAT(GlobalCpio::GetGlobalCpio(), NotNull());
  EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(options));
}

TEST(LibCpioTest, StopSuccessfully) {
  TestCpioOptions options;
  options.log_option = LogOption::kSysLog;
  options.region = kRegion;
  ASSERT_SUCCESS(TestLibCpio::InitCpio(options));
  auto cpu_async_executor = GlobalCpio::GetGlobalCpio()->GetCpuAsyncExecutor();
  ASSERT_TRUE(cpu_async_executor.ok());
  ASSERT_SUCCESS(TestLibCpio::ShutdownCpio(options));
}

TEST(LibCpioTest, InitializedCpioSucceedsTest) {
  TestCpioOptions options;
  options.log_option = LogOption::kSysLog;
  options.region = kRegion;

  MetricClientOptions metric_client_options;
  std::unique_ptr<MetricClientInterface> metric_client =
      MetricClientFactory::Create(std::move(metric_client_options));

  ASSERT_SUCCESS(TestLibCpio::InitCpio(options));
  ASSERT_SUCCESS(metric_client->Init());
  EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(options));
}

TEST(LibCpioDeathTest, UninitializedCpioFailsTest) {
  // Named "*DeathTest" to be run first for GlobalCpio static state.
  // https://github.com/google/googletest/blob/main/docs/advanced.md#death-tests-and-threads
  MetricClientOptions metric_client_options;
  std::unique_ptr<MetricClientInterface> metric_client =
      MetricClientFactory::Create(std::move(metric_client_options));

  constexpr char expected_uninit_cpio_error_message[] =
      "Cpio must be initialized with Cpio::InitCpio before client use";
  ASSERT_DEATH(metric_client->Init(), expected_uninit_cpio_error_message);
}

TEST(LibCpioDeathTest, InitAndShutdownThenInitCpioSucceedsTest) {
  TestCpioOptions options;
  options.log_option = LogOption::kSysLog;
  options.region = kRegion;

  MetricClientOptions metric_client_options;
  std::unique_ptr<MetricClientInterface> metric_client =
      MetricClientFactory::Create(std::move(metric_client_options));

  constexpr char expected_uninit_cpio_error_message[] =
      "Cpio must be initialized with Cpio::InitCpio before client use";
  ASSERT_DEATH(metric_client->Init(), expected_uninit_cpio_error_message);

  ASSERT_SUCCESS(TestLibCpio::InitCpio(options));
  ASSERT_SUCCESS(metric_client->Init());
  EXPECT_SUCCESS(TestLibCpio::ShutdownCpio(options));

  ASSERT_DEATH(metric_client->Init(), expected_uninit_cpio_error_message);
}
}  // namespace
}  // namespace google::scp::cpio::test
