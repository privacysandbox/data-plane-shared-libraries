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

#include "component_dependency_node_collection.h"

#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_set>
#include <vector>

#include "component_dependency_node.h"

namespace google::scp::core {

bool ComponentDependencyNodeCollection::AddNodeToPath(
    ComponentDependencyNode node) noexcept {
  dependency_path_.push_back(node);
  if (dependency_path_ids_.count(node.id) > 0) {
    return false;
  }
  dependency_path_ids_.insert(node.id);
  return true;
}

std::vector<ComponentDependencyNode>
ComponentDependencyNodeCollection::GetCyclePath() noexcept {
  int startCycle = -1;
  std::string lastId = dependency_path_.back().id;
  while (dependency_path_[++startCycle].id != lastId) {}

  return std::vector<ComponentDependencyNode>(
      dependency_path_.begin() + startCycle, dependency_path_.end());
}

bool ComponentDependencyNodeCollection::WasVisited(
    ComponentDependencyNode node) noexcept {
  return visited_.count(node) > 0;
}

void ComponentDependencyNodeCollection::MarkVisited() noexcept {
  auto node = dependency_path_.back();
  visited_.insert(node);
  visited_path_.push_back(node);
  dependency_path_.pop_back();
}

std::vector<ComponentDependencyNode>
ComponentDependencyNodeCollection::GetVisitedOrder() noexcept {
  return visited_path_;
}
};  // namespace google::scp::core
