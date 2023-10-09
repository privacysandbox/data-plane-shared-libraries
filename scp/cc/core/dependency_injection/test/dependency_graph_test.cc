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

#include "core/dependency_injection/src/dependency_graph.h"

#include <gtest/gtest.h>

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/dependency_injection/mock/mock_service.h"
#include "core/dependency_injection/src/component_dependency_node_collection.h"
#include "core/dependency_injection/src/error_codes.h"
#include "core/interface/logger_interface.h"
#include "core/logger/src/logger.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"

using google::scp::core::mock::MockService;
using google::scp::core::mock::TestNode;
using std::function;
using std::make_shared;
using std::map;
using std::shared_ptr;
using std::string;
using std::vector;

namespace google::scp::core::test {

/**
 * @brief Asserts that the expected ids are in the same order as the ids of the
 * component dependency nodes.
 *
 */
#define EXPECT_NODE_ORDER(expected_id_order, component_dependency_nodes)  \
  EXPECT_EQ(expected_id_order.size(), component_dependency_nodes.size()); \
  for (size_t i = 0; i < expected_id_order.size(); i++) {                 \
    EXPECT_EQ(expected_id_order[i], component_dependency_nodes[i].id);    \
  }

TEST(DependencyGraphTest, AddNodeReturnsTrueForNewNode) {
  TestNode node("A", vector<string>{"B", "C"});
  DependencyGraph target;
  bool result = target.AddNode(node.id, node.dependencies, node.factory);
  EXPECT_TRUE(result);
}

TEST(DependencyGraphTest, AddNodeReturnsFalseForExistingNode) {
  TestNode node("A", vector<string>{"B", "C"});
  DependencyGraph target;
  target.AddNode(node.id, node.dependencies, node.factory);
  bool result = target.AddNode(node.id, node.dependencies, node.factory);
  EXPECT_FALSE(result);
}

TEST(DependencyGraphTest, EnumerateReturnsCorrectBuildOrder) {
  auto test_nodes = vector<TestNode>{TestNode("A", vector<string>{"B", "C"}),
                                     TestNode("B", vector<string>{"C"}),
                                     TestNode("C", vector<string>())};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }

  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);
  EXPECT_SUCCESS(result);

  vector<string> expected_order{"C", "B", "A"};
  EXPECT_NODE_ORDER(expected_order, graph_result.dependency_order);
}

TEST(DependencyGraphTest, EnumerateReturnsUndefinedDependency) {
  auto test_nodes = vector<TestNode>{TestNode("A", vector<string>{"B", "C"}),
                                     TestNode("B", vector<string>{"C"}),
                                     TestNode("C", vector<string>{"D"}),
                                     TestNode("D", vector<string>{"E"})};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }

  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);
  EXPECT_THAT(result,
              ResultIs(FailureExecutionResult(
                  errors::SC_DEPENDENCY_INJECTION_UNDEFINED_DEPENDENCY)));
  EXPECT_EQ("E", graph_result.undefined_component);
}

// Cycle - B -> C -> D -> B
TEST(DependencyGraphTest, EnumerateReturnsCycleForCycleDetected) {
  auto test_nodes = vector<TestNode>{TestNode("A", vector<string>{"B", "C"}),
                                     TestNode("B", vector<string>{"C"}),
                                     TestNode("C", vector<string>{"D"}),
                                     TestNode("D", vector<string>{"B"})};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }
  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_DEPENDENCY_INJECTION_CYCLE_DETECTED)));

  vector<string> expected_order{"B", "C", "D", "B"};
  EXPECT_NODE_ORDER(expected_order, graph_result.detected_cycle);
}

// Graph
//        A
//     B     C
//    D  E  F H
//   G   C  I
// I H E

TEST(DependencyGraphTest, EnumerateReturnsCorrectBuildOrderForBigTree) {
  auto test_nodes =
      vector<TestNode>{TestNode("A", vector<string>{"B", "C"}),
                       TestNode("B", vector<string>{"D", "E"}),
                       TestNode("C", vector<string>{"F", "H"}),
                       TestNode("D", vector<string>{"G"}),
                       TestNode("E", vector<string>{"C"}),
                       TestNode("F", vector<string>{"I"}),
                       TestNode("G", vector<string>{"E", "H", "I"}),
                       TestNode("H", vector<string>()),
                       TestNode("I", vector<string>())};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }
  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);

  EXPECT_SUCCESS(result);
  vector<string> expected_order{"I", "F", "H", "C", "E", "G", "D", "B", "A"};
  EXPECT_NODE_ORDER(expected_order, graph_result.dependency_order);
}

// Graph
//       A
//    B     C
//   D E   F H
//  G     G
// I H E
// Cycle - G -> E -> C -> F -> G
TEST(DependencyGraphTest, EnumerateReturnsCycleForLongCycleDetected) {
  auto test_nodes =
      vector<TestNode>{TestNode("A", vector<string>{"B", "C"}),
                       TestNode("B", vector<string>{"D", "E"}),
                       TestNode("C", vector<string>{"F", "H"}),
                       TestNode("D", vector<string>{"G"}),
                       TestNode("E", vector<string>{"C"}),
                       TestNode("F", vector<string>{"G"}),
                       TestNode("G", vector<string>{"E", "H", "I"}),
                       TestNode("H", vector<string>()),
                       TestNode("I", vector<string>())};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }

  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);

  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_DEPENDENCY_INJECTION_CYCLE_DETECTED)));

  vector<string> expected_order{"G", "E", "C", "F", "G"};
  EXPECT_NODE_ORDER(expected_order, graph_result.detected_cycle);
}
}  // namespace google::scp::core::test
