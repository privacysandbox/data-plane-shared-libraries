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

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "public/core/interface/execution_result.h"
#include "scp/cc/core/interface/dependency_injection_service_interface.h"

#include "dependency_graph.h"

namespace google::scp::core {
/**
 * @brief The DependencyInjectionService provides basic dependency injection
 * functionality for components that implement ServiceInterface.  Components are
 * stored as singletons and are eagerly initialized.
 *
 */
class DependencyInjectionService : public DependencyInjectionServiceInterface {
 public:
  /**
   * @brief Registers a component by id, by specifying its dependencies and
   * creation factory.
   *
   * @param id The id of the component.
   * @param dependencies The ids of the other components that this component
   * depends on.
   * @param factory The delegate factory used to create the component.
   * @return ExecutionResult
   */
  ExecutionResult RegisterComponent(
      const std::string& id, const std::vector<std::string>& dependencies,
      std::function<std::shared_ptr<ServiceInterface>(
          const absl::flat_hash_map<std::string,
                                    std::shared_ptr<ServiceInterface>>&)>
          factory) noexcept override;
  ExecutionResult ResolveAll() noexcept override;
  ExecutionResult Init() noexcept override;
  ExecutionResult Run() noexcept override;
  ExecutionResult Stop() noexcept override;

 private:
  ExecutionResult ResolveDependencies(
      const std::vector<ComponentDependencyNode>& dependency_order);
  ExecutionResult Execute(
      std::function<ExecutionResult(std::shared_ptr<ServiceInterface>)> exec);
  DependencyGraph dependency_graph_;
  std::vector<std::shared_ptr<ServiceInterface>> components_;
  absl::flat_hash_map<std::string, std::shared_ptr<ServiceInterface>>
      component_map_;
};
}  // namespace google::scp::core
