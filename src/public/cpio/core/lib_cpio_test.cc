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
#include "src/core/async_executor/async_executor.h"
#include "src/core/async_executor/error_codes.h"
#include "src/core/common/global_logger/global_logger.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/core/test_execution_result_matchers.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/interface/metric_client/metric_client_interface.h"
#include "src/public/cpio/interface/metric_client/type_def.h"
#include "src/public/cpio/test/global_cpio/test_cpio_options.h"
#include "src/public/cpio/test/global_cpio/test_lib_cpio.h"

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
  TestCpioOptions options{.options = {.log_option = LogOption::kConsoleLog,
                                      .region = std::string{kRegion}}};
  TestLibCpio::InitCpio(options);
  TestLibCpio::ShutdownCpio(options);
}

TEST(LibCpioTest, ConsoleLogTest) {
  TestCpioOptions options{.options = {.log_option = LogOption::kConsoleLog,
                                      .region = std::string{kRegion}}};
  TestLibCpio::InitCpio(options);
  TestLibCpio::ShutdownCpio(options);
}

TEST(LibCpioTest, SysLogTest) {
  TestCpioOptions options{.options = {.log_option = LogOption::kConsoleLog,
                                      .region = std::string{kRegion}}};
  TestLibCpio::InitCpio(options);
  TestLibCpio::ShutdownCpio(options);
}

TEST(LibCpioTest, StopSuccessfully) {
  TestCpioOptions options{.options = {.log_option = LogOption::kConsoleLog,
                                      .region = std::string{kRegion}}};
  TestLibCpio::InitCpio(options);
  GlobalCpio::GetGlobalCpio().GetCpuAsyncExecutor();
  TestLibCpio::ShutdownCpio(options);
}

TEST(LibCpioTest, InitializedCpioSucceedsTest) {
  TestCpioOptions options{.options = {.log_option = LogOption::kConsoleLog,
                                      .region = std::string{kRegion}}};

  TestLibCpio::InitCpio(options);
  MetricClientOptions metric_client_options;
  std::unique_ptr<MetricClientInterface> metric_client =
      MetricClientFactory::Create(std::move(metric_client_options));
  ASSERT_TRUE(metric_client->Init().ok());
  TestLibCpio::ShutdownCpio(options);
}

TEST(LibCpioDeathTest, UninitializedCpioFailsTest) {
  // Named "*DeathTest" to be run first for GlobalCpio static state.
  // https://github.com/google/googletest/blob/main/docs/advanced.md#death-tests-and-threads
  MetricClientOptions metric_client_options;
  ASSERT_DEATH(
      MetricClientFactory::Create(std::move(metric_client_options)),
      "Cpio must be initialized with Cpio::InitCpio before client use");
}
}  // namespace
}  // namespace google::scp::cpio::test
