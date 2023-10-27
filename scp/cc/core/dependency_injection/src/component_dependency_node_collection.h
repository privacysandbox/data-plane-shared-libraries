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

#ifndef CORE_DEPENDENCY_INJECTION_SRC_COMPONENT_DEPENDENCY_NODE_COLLECTION_H_
#define CORE_DEPENDENCY_INJECTION_SRC_COMPONENT_DEPENDENCY_NODE_COLLECTION_H_
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "public/core/interface/execution_result.h"

#include "component_dependency_node.h"

namespace google::scp::core {
/**
 * @brief A collection for all the ComponentDependencyNodes that are discovered
 * in the graph.  It holds the detected cycles and the visited (non-cycle
 * confirmed) nodes separately.
 *
 */
class ComponentDependencyNodeCollection {
 public:
  /**
   * @brief Adds a node to the path and returns false if a cycle was
   * detected.
   *
   * @param node The identifier of the node to add to the collection.
   * @returns True if this node was added for the first time.
   */
  bool AddNodeToPath(ComponentDependencyNode node) noexcept;

  /**
   * @brief Gets the detected cycle.  This is called if AddNode returns false.
   *
   */
  std::vector<ComponentDependencyNode> GetCyclePath() noexcept;

  /**
   * @brief Returns true if the node was marked visited before.
   *
   * @param node The node to check that was visited.
   */
  bool WasVisited(const ComponentDependencyNode node) noexcept;

  /**
   * @brief Marks the last node added to the path as visited.  This means that
   * this node as well as all of its dependent nodes have been visited.
   *
   */
  void MarkVisited() noexcept;

  /**
   * @brief Gets the nodes in the order in which they were visited.
   *
   * @return std::vector<ComponentDependencyNode>
   */
  std::vector<ComponentDependencyNode> GetVisitedOrder() noexcept;

 private:
  absl::flat_hash_set<std::string> dependency_path_ids_;
  std::vector<ComponentDependencyNode> dependency_path_;
  absl::flat_hash_set<ComponentDependencyNode> visited_;
  std::vector<ComponentDependencyNode> visited_path_;
};
}  // namespace google::scp::core

#endif  // CORE_DEPENDENCY_INJECTION_SRC_COMPONENT_DEPENDENCY_NODE_COLLECTION_H_
