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

#include "src/public/cpio/interface/cpio.h"

#include <utility>

#include "src/core/common/global_logger/global_logger.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/interface/type_def.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::cpio::CpioOptions;
using google::scp::cpio::client_providers::CpioProviderFactory;
using google::scp::cpio::client_providers::GlobalCpio;

namespace google::scp::cpio {

ExecutionResult Cpio::InitCpio(CpioOptions options) {
  InitializeCpioLog(options.log_option);
  auto cpio = CpioProviderFactory::Create(std::move(options));
  if (!cpio.ok()) {
    return core::FailureExecutionResult(SC_UNKNOWN);
  }
  GlobalCpio::SetGlobalCpio(*std::move(cpio));
  return SuccessExecutionResult();
}

ExecutionResult Cpio::ShutdownCpio(CpioOptions options) {
  GlobalCpio::ShutdownGlobalCpio();
  return SuccessExecutionResult();
}

}  // namespace google::scp::cpio
