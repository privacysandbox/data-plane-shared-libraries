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
#include <string>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_set.h"
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
using std::shared_ptr;
using std::vector;
using ::testing::Contains;

namespace google::scp::core::test {

/**
 * @brief Asserts that the expected ids are in the same order as the ids of the
 * component dependency nodes.
 *
 */
#define EXPECT_NODE_ORDER(expected_id_order, component_dependency_nodes)  \
  ASSERT_EQ(expected_id_order.size(), component_dependency_nodes.size()); \
  for (size_t i = 0; i < expected_id_order.size(); i++) {                 \
    EXPECT_EQ(expected_id_order[i], component_dependency_nodes[i].id);    \
  }

/**
 * @brief Asserts that the nodes form a cycle with the expected edges.
 *
 */
#define EXPECT_CYCLE(expected_edges, nodes)            \
  EXPECT_EQ(expected_edges.size() + 1, nodes.size());  \
  EXPECT_EQ(nodes.front(), nodes.back());              \
  for (std::size_t i = 0; i < nodes.size() - 1; ++i) { \
    const auto it = expected_edges.find(nodes[i].id);  \
    ASSERT_NE(it, expected_edges.end());               \
    EXPECT_EQ(it->second, nodes[i + 1].id);            \
  }

TEST(DependencyGraphTest, AddNodeReturnsTrueForNewNode) {
  TestNode node("A", vector<std::string>{"B", "C"});
  DependencyGraph target;
  bool result = target.AddNode(node.id, node.dependencies, node.factory);
  EXPECT_TRUE(result);
}

TEST(DependencyGraphTest, AddNodeReturnsFalseForExistingNode) {
  TestNode node("A", vector<std::string>{"B", "C"});
  DependencyGraph target;
  target.AddNode(node.id, node.dependencies, node.factory);
  bool result = target.AddNode(node.id, node.dependencies, node.factory);
  EXPECT_FALSE(result);
}

TEST(DependencyGraphTest, EnumerateReturnsCorrectBuildOrder) {
  auto test_nodes =
      vector<TestNode>{TestNode("A", vector<std::string>{"B", "C"}),
                       TestNode("B", vector<std::string>{"C"}),
                       TestNode("C", vector<std::string>())};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }

  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);
  EXPECT_SUCCESS(result);

  vector<std::string> expected_order{"C", "B", "A"};
  EXPECT_NODE_ORDER(expected_order, graph_result.dependency_order);
}

TEST(DependencyGraphTest, EnumerateReturnsUndefinedDependency) {
  auto test_nodes =
      vector<TestNode>{TestNode("A", vector<std::string>{"B", "C"}),
                       TestNode("B", vector<std::string>{"C"}),
                       TestNode("C", vector<std::string>{"D"}),
                       TestNode("D", vector<std::string>{"E"})};

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
  const auto test_nodes = vector<TestNode>{
      TestNode("A", vector<std::string>{"B", "C"}),
      TestNode("B", vector<std::string>{"C"}),
      TestNode("C", vector<std::string>{"D"}),
      TestNode("D", vector<std::string>{"B"}),
  };

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }
  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);
  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_DEPENDENCY_INJECTION_CYCLE_DETECTED)));

  const absl::flat_hash_map<std::string, std::string> expected_edges{
      {"B", "C"}, {"C", "D"}, {"D", "B"}};
  EXPECT_CYCLE(expected_edges, graph_result.detected_cycle);
}

// Graph
//        A
//     B     C
//    D  E  F H
//   G   C  I
// I H E

TEST(DependencyGraphTest, EnumerateReturnsCorrectBuildOrderForBigTree) {
  auto test_nodes =
      vector<TestNode>{TestNode("A", vector<std::string>{"B", "C"}),
                       TestNode("B", vector<std::string>{"D", "E"}),
                       TestNode("C", vector<std::string>{"F", "H"}),
                       TestNode("D", vector<std::string>{"G"}),
                       TestNode("E", vector<std::string>{"C"}),
                       TestNode("F", vector<std::string>{"I"}),
                       TestNode("G", vector<std::string>{"E", "H", "I"}),
                       TestNode("H", vector<std::string>()),
                       TestNode("I", vector<std::string>())};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }
  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);

  EXPECT_SUCCESS(result);
  absl::flat_hash_set<std::string> visited_nodes;

  // All nodes should be visited.
  EXPECT_EQ(test_nodes.size(), graph_result.dependency_order.size());
  for (const auto& node : graph_result.dependency_order) {
    const auto it = absl::c_find_if(test_nodes, [&](const TestNode& test_node) {
      return test_node.id == node.id;
    });
    ASSERT_NE(it, test_nodes.end());

    // All dependencies should already be visited.
    for (const std::string& dependency : it->dependencies) {
      EXPECT_THAT(visited_nodes, Contains(dependency));
    }

    // Node should not be visited twice.
    const auto [_, success] = visited_nodes.insert(node.id);
    EXPECT_TRUE(success);
  }
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
      vector<TestNode>{TestNode("A", vector<std::string>{"B", "C"}),
                       TestNode("B", vector<std::string>{"D", "E"}),
                       TestNode("C", vector<std::string>{"F", "H"}),
                       TestNode("D", vector<std::string>{"G"}),
                       TestNode("E", vector<std::string>{"C"}),
                       TestNode("F", vector<std::string>{"G"}),
                       TestNode("G", vector<std::string>{"E", "H", "I"}),
                       TestNode("H", vector<std::string>()),
                       TestNode("I", vector<std::string>())};

  DependencyGraph target;
  for (auto node : test_nodes) {
    target.AddNode(node.id, node.dependencies, node.factory);
  }

  DependencyGraphEnumerationResult graph_result;
  auto result = target.Enumerate(graph_result);

  EXPECT_THAT(result, ResultIs(FailureExecutionResult(
                          errors::SC_DEPENDENCY_INJECTION_CYCLE_DETECTED)));

  const absl::flat_hash_map<std::string, std::string> expected_edges{
      {"G", "E"}, {"E", "C"}, {"C", "F"}, {"F", "G"}};
  EXPECT_CYCLE(expected_edges, graph_result.detected_cycle);
}
}  // namespace google::scp::core::test
