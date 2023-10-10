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

#ifndef CORE_DEPENDENCY_INJECTION_SRC_DEPENDENCY_GRAPH_INTERFACE_H_
#define CORE_DEPENDENCY_INJECTION_SRC_DEPENDENCY_GRAPH_INTERFACE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "public/core/interface/execution_result.h"
#include "scp/cc/core/dependency_injection/src/component_dependency_node.h"
#include "scp/cc/core/interface/service_interface.h"

namespace google::scp::core {

///  A result of an enumeration through the dependency graph.
struct DependencyGraphEnumerationResult {
  /**
   * @brief A vector of nodes representing the order in which one can safely
   * build components.
   *
   */
  std::vector<ComponentDependencyNode> dependency_order;

  /**
   * @brief  A vector of nodes representing the order of a detected cycle in the
   * dependency graph.
   *
   */
  std::vector<ComponentDependencyNode> detected_cycle;

  /**
   * @brief The component that was provided as a dependency but was not added as
   * a node.
   *
   */
  std::string undefined_component;
};

/// The interface for a graph representing component dependencies.
class DependencyGraphInterface {
 public:
  /**
   * @brief Adds a component, it's dependencies and its factory to the graph.
   *
   * @param id The id of the component.
   * @param dependencies The ids of the dependencies for this component.
   * @param factory The delegate that creates this component.
   *
   * @returns True if the component has not been added before.
   */
  virtual bool AddNode(
      const std::string& id, const std::vector<std::string>& dependencies,
      std::function<std::shared_ptr<ServiceInterface>(
          const absl::flat_hash_map<std::string,
                                    std::shared_ptr<ServiceInterface>>&)>
          factory) noexcept = 0;

  /**
   * @brief Enumerates the nodes in the graph and returns a result based on
   * the enumeration.
   *
   * @return DependencyGraphEnumerationResult the result of the enumeration
   * corresponding to the proper build order of the components or an error
   * that was found.
   */
  virtual ExecutionResult Enumerate(
      DependencyGraphEnumerationResult& result) noexcept = 0;
};

}  // namespace google::scp::core

#endif  // CORE_DEPENDENCY_INJECTION_SRC_DEPENDENCY_GRAPH_INTERFACE_H_
