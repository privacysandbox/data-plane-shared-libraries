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

#include "component_factory.h"

#include <memory>
#include <utility>

#include "core/common/global_logger/src/global_logger.h"
#include "cpio/server/interface/component_factory/component_factory_interface.h"
#include "scp/cc/core/common/uuid/src/uuid.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::ServiceInterface;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::common::kZeroUuid;

namespace {
constexpr char kComponentFactory[] = "ComponentFactory";
}  // namespace

namespace google::scp::cpio {
ExecutionResult ComponentFactory::Init() noexcept {
  for (auto& creator : component_creators_) {
    ASSIGN_OR_LOG_AND_RETURN(creator.component, creator.creation_func(),
                             kComponentFactory, kZeroUuid,
                             "Failed to create component %s.",
                             creator.component_name.c_str());
    RETURN_AND_LOG_IF_FAILURE(creator.component->Init(), kComponentFactory,
                              kZeroUuid, "Failed to init component %s.",
                              creator.component_name.c_str());
  }

  return SuccessExecutionResult();
}

ExecutionResult ComponentFactory::Run() noexcept {
  for (auto& creator : component_creators_) {
    RETURN_AND_LOG_IF_FAILURE(creator.component->Run(), kComponentFactory,
                              kZeroUuid, "Failed to run component %s.",
                              creator.component_name.c_str());
  }

  return SuccessExecutionResult();
}

ExecutionResult ComponentFactory::Stop() noexcept {
  // Stop components in the reverse order.
  for (auto it = component_creators_.rbegin(); it != component_creators_.rend();
       ++it) {
    RETURN_AND_LOG_IF_FAILURE(it->component->Stop(), kComponentFactory,
                              kZeroUuid, "Failed to stop component %s.",
                              it->component_name.c_str());
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::cpio
