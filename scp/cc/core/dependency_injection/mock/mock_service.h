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

#ifndef CORE_DEPENDENCY_INJECTION_MOCK_MOCK_SERVICE_H_
#define CORE_DEPENDENCY_INJECTION_MOCK_MOCK_SERVICE_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "scp/cc/core/interface/service_interface.h"

namespace google::scp::core::mock {
/**
 * @brief A ServiceInterface that has an identifier.  Used for testing purposes.
 *
 */
class IdentifiableService : public ServiceInterface {
 public:
  virtual std::string GetId() = 0;
};

/**
 * @brief Keeps track of the order that MockServices have been created.
 *
 */
class ServiceCollection {
 public:
  /**
   * @brief Adds a IdentifiableService to the collection.
   *
   * @param service The service to add.
   */
  void Add(std::shared_ptr<IdentifiableService>& service) {
    int service_order = order++;
    auto id = service.get()->GetId();
    collection_map[id] = service;
    collection_order[id] = service_order;
  }

  /**
   * @brief Gets the order in which a given service was added to the collection.
   *
   * @param id The id of the service.
   * @return int The order the service was added.
   */
  int GetInstantiationOrder(std::string& id) { return collection_order[id]; }

  /**
   * @brief Gets a given MockService from the collection.
   *
   * @param id The id of the MockService.
   * @return std::shared_ptr<MockService> A shared pointer to the MockService.
   */
  std::shared_ptr<IdentifiableService> GetService(std::string& id) {
    return collection_map[id];
  }

  /// Resets the collection
  void Reset() {
    collection_map.clear();
    collection_order.clear();
    order = 1;
  }

  /// The singleton instance of MockServiceCollection.
  static ServiceCollection collection;

 private:
  int order = 1;
  absl::flat_hash_map<std::string, std::shared_ptr<IdentifiableService>>
      collection_map;
  absl::flat_hash_map<std::string, int> collection_order;
};

/// Set aside memory for static collection;
ServiceCollection ServiceCollection::collection;

/// Provides a mock testing class for the ServiceInterface.
class MockService : public IdentifiableService {
 public:
  /**
   * @brief Construct a new Mock Service object
   *
   * @param id The id of this component.
   */
  explicit MockService(std::string id) : id(id) {}

  /// Gets the id used to identify this mock.
  std::string GetId() { return id; }

  /// Returns true if Init() was called.
  bool WasInit() { return init; }

  /// Returns true if Run() was called.
  bool WasRun() { return run; }

  /// Returns true if Stop() was called.
  bool WasStopped() { return stop; }

  ExecutionResult Init() noexcept {
    if (execution_result.status == ExecutionStatus::Success) {
      init = true;
    }
    return execution_result;
  }

  ExecutionResult Run() noexcept {
    if (execution_result.status == ExecutionStatus::Success) {
      run = true;
    }
    return execution_result;
  }

  ExecutionResult Stop() noexcept {
    if (execution_result.status == ExecutionStatus::Success) {
      stop = true;
    }
    return execution_result;
  }

  /// Sets the next execution result to be returned by any of the
  /// ServiceInterface methods.
  ExecutionResult SetNextResult(ExecutionResult result) {
    execution_result = result;
    return result;
  }

 private:
  std::string id;
  ExecutionResult execution_result = SuccessExecutionResult();
  bool init = false;
  bool run = false;
  bool stop = false;
};

/// A TestNode used to test dependency graph registrations.
struct TestNode {
  /**
   * @brief Construct a new TestNode object.
   *
   * @param id The id used to identify the component.
   * @param deps The component ids used to identify its dependencies.
   */
  TestNode(std::string id, std::vector<std::string> deps) : id(id) {
    dependencies = deps;
    factory =
        [id](absl::flat_hash_map<std::string, std::shared_ptr<ServiceInterface>>
                 compMap) {
          std::shared_ptr<IdentifiableService> mock_service =
              std::make_shared<MockService>(id);
          ServiceCollection::collection.Add(mock_service);
          return mock_service;
        };
    // Factory used to simulate an exception thrown during component creation.
    fail_factory =
        [id](absl::flat_hash_map<std::string, std::shared_ptr<ServiceInterface>>
                 compMap) {
          throw std::invalid_argument("");
          std::shared_ptr<IdentifiableService> mock_service =
              std::make_shared<MockService>(id);
          ServiceCollection::collection.Add(mock_service);
          return mock_service;
        };
  }

  /// Verifies that the ComponentDependencyNode has the same data registered
  /// with the dependency graph.
  bool operator==(const ComponentDependencyNode& otherNode) const {
    // Test that dependencies and factory are equal by reference
    return id == otherNode.id && dependencies == otherNode.dependencies;
  }

  std::string id;
  std::vector<std::string> dependencies;
  std::function<std::shared_ptr<ServiceInterface>(
      absl::flat_hash_map<std::string, std::shared_ptr<ServiceInterface>>)>
      factory;
  std::function<std::shared_ptr<ServiceInterface>(
      absl::flat_hash_map<std::string, std::shared_ptr<ServiceInterface>>)>
      fail_factory;
};
}  // namespace google::scp::core::mock

#endif  // CORE_DEPENDENCY_INJECTION_MOCK_MOCK_SERVICE_H_
