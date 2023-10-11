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

#include "scp/cc/core/dependency_injection/src/dependency_injection_service.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/dependency_injection/mock/mock_service.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "scp/cc/core/dependency_injection/src/error_codes.h"
#include "scp/cc/core/interface/dependency_injection_service_interface.h"

using std::function;
using std::make_shared;
using std::shared_ptr;
using std::unordered_set;

using google::scp::core::errors::
    SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED;
using google::scp::core::errors::
    SC_DEPENDENCY_INJECTION_ERROR_CREATING_COMPONENTS;
using google::scp::core::mock::MockService;
using google::scp::core::mock::ServiceCollection;
using google::scp::core::mock::TestNode;
using std::reinterpret_pointer_cast;

namespace google::scp::core::test {

/// Used to call a SetUp method to run before each test.
class DependencyInjectionServiceTest : public testing::Test {
 protected:
  void SetUp() override { ServiceCollection::collection.Reset(); }
};

/**
 * @brief Syntactic sugar to get a Mock Service from the global service
 * collection.
 *
 * @param id The id of the service.
 * @return MockService The service
 */
shared_ptr<MockService> GetMockService(std::string id) {
  return reinterpret_pointer_cast<MockService>(
      (ServiceCollection::collection.GetService(id)));
}

TEST_F(DependencyInjectionServiceTest, RegisterComponentOnceReturnsSuccess) {
  DependencyInjectionService service;
  TestNode node("A", std::vector<std::string>{"B", "C"});

  auto result1 =
      service.RegisterComponent(node.id, node.dependencies, node.factory);

  EXPECT_SUCCESS(result1);
}

TEST_F(DependencyInjectionServiceTest, RegisterComponentTwiceReturnsError) {
  DependencyInjectionService service;
  TestNode node("A", std::vector<std::string>{"B", "C"});

  auto result1 =
      service.RegisterComponent(node.id, node.dependencies, node.factory);
  auto result2 =
      service.RegisterComponent(node.id, node.dependencies, node.factory);

  EXPECT_SUCCESS(result1);
  EXPECT_THAT(
      result2,
      ResultIs(FailureExecutionResult(
          errors::SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED)));
}

TEST_F(DependencyInjectionServiceTest,
       ResolveAllCreatesComponentsInCorrectOrder) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  auto result = service.ResolveAll();

  EXPECT_SUCCESS(result);

  int order_a = ServiceCollection::collection.GetInstantiationOrder(node_a.id);
  int order_b = ServiceCollection::collection.GetInstantiationOrder(node_b.id);
  int order_c = ServiceCollection::collection.GetInstantiationOrder(node_c.id);
  int order_d = ServiceCollection::collection.GetInstantiationOrder(node_d.id);

  EXPECT_EQ(4, order_a);
  EXPECT_EQ(3, order_b);
  EXPECT_EQ(2, order_c);
  EXPECT_EQ(1, order_d);
}

TEST_F(DependencyInjectionServiceTest,
       ResolveAllReturnsFailureWhenFactoryThrowsException) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies,
                            node_c.fail_factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  auto result = service.ResolveAll();
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  errors::SC_DEPENDENCY_INJECTION_ERROR_CREATING_COMPONENTS)));
}

TEST_F(DependencyInjectionServiceTest, InitInitializesComponents) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  service.ResolveAll();
  auto result = service.Init();

  EXPECT_SUCCESS(result);

  auto service_a = GetMockService(node_a.id);
  auto service_b = GetMockService(node_b.id);
  auto service_c = GetMockService(node_c.id);
  auto service_d = GetMockService(node_d.id);

  EXPECT_TRUE(service_a.get()->WasInit());
  EXPECT_TRUE(service_b.get()->WasInit());
  EXPECT_TRUE(service_c.get()->WasInit());
  EXPECT_TRUE(service_d.get()->WasInit());
}

TEST_F(DependencyInjectionServiceTest, RunRunsComponents) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  service.ResolveAll();
  auto result = service.Run();

  EXPECT_SUCCESS(result);

  auto service_a = GetMockService(node_a.id);
  auto service_b = GetMockService(node_b.id);
  auto service_c = GetMockService(node_c.id);
  auto service_d = GetMockService(node_d.id);

  EXPECT_TRUE(service_a.get()->WasRun());
  EXPECT_TRUE(service_b.get()->WasRun());
  EXPECT_TRUE(service_c.get()->WasRun());
  EXPECT_TRUE(service_d.get()->WasRun());
}

TEST_F(DependencyInjectionServiceTest, StopStopsComponents) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  service.ResolveAll();
  auto result = service.Stop();

  EXPECT_SUCCESS(result);

  auto service_a = GetMockService(node_a.id);
  auto service_b = GetMockService(node_b.id);
  auto service_c = GetMockService(node_c.id);
  auto service_d = GetMockService(node_d.id);

  EXPECT_TRUE(service_a.get()->WasStopped());
  EXPECT_TRUE(service_b.get()->WasStopped());
  EXPECT_TRUE(service_c.get()->WasStopped());
  EXPECT_TRUE(service_d.get()->WasStopped());
}

TEST_F(DependencyInjectionServiceTest,
       StopReturnsErrorWhenServiceReturnsError) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  service.ResolveAll();

  auto service_a = GetMockService(node_a.id);
  auto service_b = GetMockService(node_b.id);
  auto service_c = GetMockService(node_c.id);
  auto service_d = GetMockService(node_d.id);

  service_b->SetNextResult(FailureExecutionResult(
      SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED));

  auto result = service.Stop();
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(
          errors::SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED)));

  EXPECT_FALSE(service_a.get()->WasStopped());
  EXPECT_FALSE(service_b.get()->WasStopped());
  EXPECT_TRUE(service_c.get()->WasStopped());
  EXPECT_TRUE(service_d.get()->WasStopped());
}

TEST_F(DependencyInjectionServiceTest,
       InitReturnsErrorWhenServiceReturnsError) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  service.ResolveAll();

  auto service_a = GetMockService(node_a.id);
  auto service_b = GetMockService(node_b.id);
  auto service_c = GetMockService(node_c.id);
  auto service_d = GetMockService(node_d.id);

  service_b->SetNextResult(FailureExecutionResult(
      SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED));

  auto result = service.Init();
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(
          errors::SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED)));

  EXPECT_FALSE(service_a.get()->WasInit());
  EXPECT_FALSE(service_b.get()->WasInit());
  EXPECT_TRUE(service_c.get()->WasInit());
  EXPECT_TRUE(service_d.get()->WasInit());
}

TEST_F(DependencyInjectionServiceTest, RunReturnsErrorWhenServiceReturnsError) {
  DependencyInjectionService service;
  TestNode node_a("A", std::vector<std::string>{"B", "C"});
  TestNode node_b("B", std::vector<std::string>{"D", "C"});
  TestNode node_c("C", std::vector<std::string>{"D"});
  TestNode node_d("D", std::vector<std::string>{});

  service.RegisterComponent(node_a.id, node_a.dependencies, node_a.factory);
  service.RegisterComponent(node_c.id, node_c.dependencies, node_c.factory);
  service.RegisterComponent(node_b.id, node_b.dependencies, node_b.factory);
  service.RegisterComponent(node_d.id, node_d.dependencies, node_d.factory);

  service.ResolveAll();

  auto service_a = GetMockService(node_a.id);
  auto service_b = GetMockService(node_b.id);
  auto service_c = GetMockService(node_c.id);
  auto service_d = GetMockService(node_d.id);

  service_b->SetNextResult(FailureExecutionResult(
      SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED));

  auto result = service.Run();
  EXPECT_THAT(
      result,
      ResultIs(FailureExecutionResult(
          errors::SC_DEPENDENCY_INJECTION_COMPONENT_ALREADY_REGISTERED)));

  EXPECT_FALSE(service_a.get()->WasRun());
  EXPECT_FALSE(service_b.get()->WasRun());
  EXPECT_TRUE(service_c.get()->WasRun());
  EXPECT_TRUE(service_d.get()->WasRun());
}
}  // namespace google::scp::core::test
