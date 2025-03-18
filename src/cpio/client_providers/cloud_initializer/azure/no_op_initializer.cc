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

#include "no_op_initializer.h"

#include <memory>

#include "src/public/core/interface/execution_result.h"

using google::scp::core::ExecutionResult;
using google::scp::core::SuccessExecutionResult;

namespace google::scp::cpio::client_providers {
void NoOpInitializer::InitCloud() noexcept {}

void NoOpInitializer::ShutdownCloud() noexcept {}

std::unique_ptr<CloudInitializerInterface> CloudInitializerFactory::Create() {
  return std::make_unique<NoOpInitializer>();
}
}  // namespace google::scp::cpio::client_providers
