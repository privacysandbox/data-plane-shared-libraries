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
#include <map>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "public/core/interface/execution_result.h"
#include "scp/cc/core/dependency_injection/src/dependency_graph_interface.h"

#include "component_dependency_node.h"
#include "component_dependency_node_collection.h"

namespace google::scp::core {
/**
 * @brief The graph used to store dependencies between components.  This is used
 * by the DependencyInjectionService to make sure components are initialized in
 * the proper order.
 *
 */
class DependencyGraph : public DependencyGraphInterface {
 public:
  /**
   * @brief Adds a dependency to the graph.
   *
   * @param id The id of the component to add.
   * @param dependencies The ids of the components that the added component
   * depends on.
   * @param factory The delegate used to create the component.
   */
  bool AddNode(
      const std::string& id, const std::vector<std::string>& dependencies,
      std::function<std::shared_ptr<ServiceInterface>(
          const std::map<std::string, std::shared_ptr<ServiceInterface>>&)>
          factory) noexcept override;
  /**
   * @brief Enumerates through the nodes in the graph returning a safe order
   * of instantiation.
   *
   * @return DependencyGraphEnumerationResult
   */
  ExecutionResult Enumerate(
      DependencyGraphEnumerationResult& enumerationResult) noexcept override;

 private:
  ExecutionResult Enumerate(const ComponentDependencyNode& node,
                            DependencyGraphEnumerationResult& result);
  ExecutionResult GetSuccess(DependencyGraphEnumerationResult& result);
  ExecutionResult GetUndefined(DependencyGraphEnumerationResult& result,
                               const std::string& dependency);
  ExecutionResult GetCycle(DependencyGraphEnumerationResult& result);
  std::map<std::string, ComponentDependencyNode> nodes_;
  ComponentDependencyNodeCollection visited_nodes_;
};
};  // namespace google::scp::core
