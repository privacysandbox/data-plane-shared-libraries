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

#include "dependency_graph.h"

#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "public/core/interface/execution_result.h"

#include "component_dependency_node.h"
#include "component_dependency_node_collection.h"
#include "error_codes.h"

namespace google::scp::core {

bool DependencyGraph::AddNode(
    const std::string& id, const std::vector<std::string>& dependencies,
    std::function<std::shared_ptr<ServiceInterface>(
        const absl::flat_hash_map<std::string,
                                  std::shared_ptr<ServiceInterface>>&)>
        factory) noexcept {
  if (nodes_.count(id) != 0) return false;

  nodes_.emplace(id, ComponentDependencyNode{id, dependencies, factory});
  return true;
}

ExecutionResult DependencyGraph::Enumerate(
    DependencyGraphEnumerationResult& result) noexcept {
  // First visit each registered node
  for (const auto& node : nodes_) {
    // Add the node to the visited path
    visited_nodes_.AddNodeToPath(node.second);
    // Now visit the nodes that it depends on
    auto enumerate_status = Enumerate(node.second, result);
    if (!enumerate_status.Successful()) {
      return enumerate_status;
    }
  }
  return GetSuccess(result);
}

ExecutionResult DependencyGraph::Enumerate(
    const ComponentDependencyNode& node,
    DependencyGraphEnumerationResult& result) {
  // If this node was already marked as visited then return success
  if (visited_nodes_.WasVisited(node)) {
    return SuccessExecutionResult();
  }

  for (const auto& dependency : node.dependencies) {
    // If a dependency has been declared by a node but has not been registered
    // with the dependency injection service then return error
    if (nodes_.count(dependency) == 0) {
      // TODO: Log undefined dependency
      return GetUndefined(result, dependency);
    }

    auto dependencyNode = nodes_[dependency];
    // If a dependency was already visited then ignore it
    if (visited_nodes_.WasVisited(dependencyNode)) {
      continue;
    }

    // If this node has already been added to the visited path then there must
    // be a cycle
    if (!visited_nodes_.AddNodeToPath(dependencyNode)) {
      // TODO: Log cycle
      return GetCycle(result);
    }

    // Now enumerate this dependent node's dependencies
    auto enumerate_status = Enumerate(dependencyNode, result);
    // If enumerating this dependent node has returned error, then return the
    // error
    if (!enumerate_status.Successful()) {
      return enumerate_status;
    }
  }
  // Only mark a node as visited when it and all of it's dependencies have been
  // marked visited
  visited_nodes_.MarkVisited();
  return SuccessExecutionResult();
}

ExecutionResult DependencyGraph::GetSuccess(
    DependencyGraphEnumerationResult& result) {
  result.dependency_order = visited_nodes_.GetVisitedOrder();
  return SuccessExecutionResult();
}

ExecutionResult DependencyGraph::GetUndefined(
    DependencyGraphEnumerationResult& result, const std::string& dependency) {
  result.undefined_component = dependency;
  return FailureExecutionResult(
      errors::SC_DEPENDENCY_INJECTION_UNDEFINED_DEPENDENCY);
}

ExecutionResult DependencyGraph::GetCycle(
    DependencyGraphEnumerationResult& result) {
  result.detected_cycle = visited_nodes_.GetCyclePath();
  return FailureExecutionResult(errors::SC_DEPENDENCY_INJECTION_CYCLE_DETECTED);
}
};  // namespace google::scp::core
