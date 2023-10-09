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

#include "core/dependency_injection/src/component_dependency_node_collection.h"

#include <gtest/gtest.h>

#include <string>

#include "public/core/interface/execution_result.h"

using std::string;

namespace google::scp::core::test {

TEST(ComponentDependencyNodeCollectionTests,
     AddNodeToPathReturnsTrueForNewNode) {
  string id = "node1";
  ComponentDependencyNodeCollection target;
  ComponentDependencyNode node{id};
  bool result = target.AddNodeToPath(node);
  EXPECT_TRUE(result);
}

TEST(ComponentDependencyNodeCollectionTests,
     AddNodeToPathReturnsFalseForExistingNode) {
  string id1 = "node1";
  string id2 = "node2";
  ComponentDependencyNodeCollection target;
  ComponentDependencyNode node1{id1};
  ComponentDependencyNode node2{id2};
  target.AddNodeToPath(node1);
  target.AddNodeToPath(node2);
  bool result = target.AddNodeToPath(node1);
  EXPECT_FALSE(result);
  auto cycle = target.GetCyclePath();
  EXPECT_EQ(3, cycle.size());
  EXPECT_EQ(node1, cycle[0]);
  EXPECT_EQ(node2, cycle[1]);
  EXPECT_EQ(node1, cycle[0]);
}

TEST(ComponentDependencyNodeCollectionTests,
     GetCyclePathReturnsTheCyclePathInOrder) {
  string id1 = "node1";
  string id2 = "node2";
  string id3 = "node3";
  string id4 = "node4";
  string id5 = "node5";
  ComponentDependencyNodeCollection target;
  ComponentDependencyNode node1{id1};
  ComponentDependencyNode node2{id2};
  ComponentDependencyNode node3{id3};
  ComponentDependencyNode node4{id4};
  ComponentDependencyNode node5{id5};
  target.AddNodeToPath(node1);
  target.AddNodeToPath(node2);
  target.AddNodeToPath(node3);
  target.AddNodeToPath(node4);
  target.AddNodeToPath(node5);
  target.AddNodeToPath(node3);
  auto cycle = target.GetCyclePath();
  EXPECT_EQ(4, cycle.size());
  EXPECT_EQ(node3, cycle[0]);
  EXPECT_EQ(node4, cycle[1]);
  EXPECT_EQ(node5, cycle[2]);
  EXPECT_EQ(node3, cycle[0]);
}

TEST(ComponentDependencyNodeCollectionTests,
     WasVisitedReturnsTrueIfMarkedVisited) {
  string id1 = "node1";

  ComponentDependencyNodeCollection target;
  ComponentDependencyNode node1{id1};
  bool preResult = target.WasVisited(node1);
  EXPECT_FALSE(preResult);
  target.AddNodeToPath(node1);
  target.MarkVisited();
  bool result = target.WasVisited(node1);
  EXPECT_TRUE(result);
}

TEST(ComponentDependencyNodeCollectionTests,
     WasVisitedReturnsFalseIfNotMarkedVisited) {
  string id1 = "node1";
  string id2 = "node2";

  ComponentDependencyNodeCollection target;
  ComponentDependencyNode node1{id1};
  ComponentDependencyNode node2{id2};
  target.AddNodeToPath(node2);
  target.AddNodeToPath(node1);
  target.MarkVisited();
  bool result1 = target.WasVisited(node1);
  bool result2 = target.WasVisited(node2);
  EXPECT_TRUE(result1);
  EXPECT_FALSE(result2);
}

TEST(ComponentDependencyNodeCollectionTests,
     GetVisitedOrderReturnsOrderOfNodesMarkedVisited) {
  string id1 = "node1";
  string id2 = "node2";
  string id3 = "node3";

  ComponentDependencyNodeCollection target;
  ComponentDependencyNode node1{id1};
  ComponentDependencyNode node2{id2};
  ComponentDependencyNode node3{id3};
  target.AddNodeToPath(node1);
  target.AddNodeToPath(node2);
  target.AddNodeToPath(node3);
  target.MarkVisited();
  target.MarkVisited();
  target.MarkVisited();
  auto visitedOrder = target.GetVisitedOrder();

  EXPECT_EQ(3, visitedOrder.size());
  EXPECT_EQ(node3, visitedOrder[0]);
  EXPECT_EQ(node2, visitedOrder[1]);
  EXPECT_EQ(node1, visitedOrder[2]);
}
}  // namespace google::scp::core::test
