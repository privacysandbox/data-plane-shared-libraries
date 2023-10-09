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

#include "dependency_injection_service.h"

#include <algorithm>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "public/core/interface/execution_result.h"

#include "dependency_graph.h"
#include "error_codes.h"
using std::for_each;
using std::function;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::unordered_set;
using std::vector;

namespace google::scp::core {

ExecutionResult DependencyInjectionService::RegisterComponent(
    const string& id, const vector<string>& dependencies,
    function<shared_ptr<ServiceInterface>(
        const map<string, shared_ptr<ServiceInterface>>&)>
        factory) noexcept {
  if (!dependency_graph_.AddNode(id, dependencies, factory)) {
    return FailureExecutionResult(
        errors::SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED);
  }
  return SuccessExecutionResult();
}

ExecutionResult DependencyInjectionService::ResolveAll() noexcept {
  DependencyGraphEnumerationResult graph_enumeration_result;
  auto enumeration_result =
      dependency_graph_.Enumerate(graph_enumeration_result);
  if (!enumeration_result.Successful()) {
    return enumeration_result;
  }
  return ResolveDependencies(graph_enumeration_result.dependency_order);
}

ExecutionResult DependencyInjectionService::Init() noexcept {
  return Execute([](auto component) { return component.get()->Init(); });
}

ExecutionResult DependencyInjectionService::Run() noexcept {
  return Execute([](auto component) { return component.get()->Run(); });
}

ExecutionResult DependencyInjectionService::Stop() noexcept {
  return Execute([](auto component) { return component.get()->Stop(); });
}

ExecutionResult DependencyInjectionService::Execute(
    function<ExecutionResult(shared_ptr<ServiceInterface>)> execute) {
  for (auto& node : components_) {
    auto result = execute(node);
    if (result.status != ExecutionStatus::Success) {
      return result;
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult DependencyInjectionService::ResolveDependencies(
    const vector<ComponentDependencyNode>& dependency_order) {
  try {
    for_each(dependency_order.begin(), dependency_order.end(),
             [this](ComponentDependencyNode node) {
               auto service = node.factory(component_map_);
               component_map_[node.id] = service;
               components_.push_back(service);
             });
  } catch (...) {
    return FailureExecutionResult(
        errors::SC_DEPENDENCY_INJECTION_ERROR_CREATING_COMPONENTS);
  }
  return SuccessExecutionResult();
}
}  // namespace google::scp::core
