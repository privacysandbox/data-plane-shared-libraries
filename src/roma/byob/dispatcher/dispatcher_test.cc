// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/byob/dispatcher/dispatcher.h"

#include <gtest/gtest.h>

namespace privacy_sandbox::server_common::byob {
namespace {
TEST(DispatcherTest, ShutdownPreInit) { Dispatcher dispatcher; }

TEST(DispatcherTest, LoadErrorsForEmptyBinaryPath) {
  Dispatcher dispatcher;
  EXPECT_FALSE(dispatcher.LoadBinary("", /*num_workers=*/1).ok());
}

// TODO: b/371538589 - Ensure non-file paths are handled appropriately.
TEST(DispatcherTest, DISABLED_LoadErrorsForRootPath) {
  Dispatcher dispatcher;
  EXPECT_FALSE(dispatcher.LoadBinary("/", /*num_workers=*/1).ok());
}

TEST(DispatcherTest, LoadErrorsForUnknownBinaryPath) {
  Dispatcher dispatcher;
  EXPECT_FALSE(
      dispatcher.LoadBinary("/asdflkj/ytrewq", /*num_workers=*/1).ok());
}

TEST(DispatcherTest, LoadErrorsWhenNWorkersNonPositive) {
  Dispatcher dispatcher;
  EXPECT_FALSE(dispatcher
                   .LoadBinary("src/roma/byob/sample_udf/new_udf",
                               /*num_workers=*/0)
                   .ok());
}
}  // namespace
}  // namespace privacy_sandbox::server_common::byob
