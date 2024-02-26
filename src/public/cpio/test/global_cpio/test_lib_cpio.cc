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

#include "test_lib_cpio.h"

#include <memory>

#include "src/cpio/client_providers/global_cpio/cpio_provider/test_lib_cpio_provider.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/core/cpio_utils.h"
#include "src/public/cpio/interface/cpio.h"
#include "src/public/cpio/test/global_cpio/test_cpio_options.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::client_providers::GlobalCpio;
using google::scp::cpio::client_providers::TestLibCpioProvider;

namespace google::scp::cpio {
static ExecutionResult SetGlobalCpio(const TestCpioOptions& options) {
  cpio_ptr = std::make_unique<TestLibCpioProvider>(options);

  CpioUtils::RunAndSetGlobalCpio(std::move(cpio_ptr));

  return SuccessExecutionResult();
}

ExecutionResult TestLibCpio::InitCpio(TestCpioOptions options) {
  auto execution_result = Cpio::InitCpio(options.ToCpioOptions(), false);
  if (!execution_result.Successful()) {
    return execution_result;
  }
  return SetGlobalCpio(options);
}

ExecutionResult TestLibCpio::ShutdownCpio(TestCpioOptions options) {
  return Cpio::ShutdownCpio(options.ToCpioOptions());
}
}  // namespace google::scp::cpio
