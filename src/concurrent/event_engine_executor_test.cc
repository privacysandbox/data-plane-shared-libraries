//  Copyright 2022 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "src/concurrent/event_engine_executor.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <random>
#include <utility>

#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "src/concurrent/mocks.h"

namespace privacy_sandbox::server_common {
namespace {
using ::testing::An;
using ::testing::DoAll;
using ::testing::Invoke;
using ::testing::Return;

using TaskHandle = grpc_event_engine::experimental::EventEngine::TaskHandle;

void FakeRun(absl::AnyInvocable<void()> method) { method(); }

TaskHandle FakeRunAfter(testing::Unused, absl::AnyInvocable<void()> method) {
  method();
  TaskHandle task_handle = {1, 2};
  return task_handle;
}

TEST(EventEngineExecutorTest, RunsUsesEventEngine) {
  auto mock_event_engine_ptr = std::make_unique<MockEventEngine>();
  auto& mock_event_engine = *mock_event_engine_ptr.get();
  EventEngineExecutor class_under_test(std::move(mock_event_engine_ptr));

  std::default_random_engine generator(ToUnixSeconds(absl::Now()));
  int value = std::uniform_int_distribution<int>()(generator);
  // expected_value != value
  int expected_val = value - 1;

  absl::AnyInvocable<void()> lambda = [value,
                                       expected_val_ptr = &expected_val]() {
    *expected_val_ptr = value;
  };

  // Capture callback passed to event engine
  EXPECT_CALL(mock_event_engine, Run(An<absl::AnyInvocable<void()>>()))
      .Times(1)
      .WillOnce(FakeRun);

  class_under_test.Run(std::move(lambda));

  // Indirectly asserts that same lambda is executed here
  EXPECT_EQ(value, expected_val);
}

TEST(EventEngineExecutorTest, RunsAfterUsesEventEngine) {
  auto mock_event_engine_ptr = std::make_unique<MockEventEngine>();
  auto& mock_event_engine = *mock_event_engine_ptr.get();
  EventEngineExecutor class_under_test(std::move(mock_event_engine_ptr));

  // generate random delay between 2 and 10 Milliseconds
  std::default_random_engine generator(ToUnixSeconds(absl::Now()));
  int random_int = std::uniform_int_distribution<int>(2, 10)(generator);
  absl::Duration delay = absl::Milliseconds(random_int);

  int value = std::uniform_int_distribution<int>()(generator);
  // expected_value != value
  int expected_val = value - 1;

  absl::AnyInvocable<void()> lambda = [value,
                                       expected_val_ptr = &expected_val]() {
    *expected_val_ptr = value;
  };

  // Capture callback passed to event engine
  EXPECT_CALL(mock_event_engine, RunAfter(ToChronoNanoseconds(delay),
                                          An<absl::AnyInvocable<void()>>()))
      .Times(1)
      .WillOnce(FakeRunAfter);

  TaskId id = class_under_test.RunAfter(delay, std::move(lambda));
  // Verify the task ID matches the return value in FakeRunAfter() above.
  EXPECT_THAT(id.keys, testing::ElementsAre(1, 2));

  // Indirectly asserts that same lambda is executed here
  EXPECT_EQ(value, expected_val);
}

TEST(EventEngineExecutorTest, CancelUsesEventEngine) {
  auto mock_event_engine_ptr = std::make_unique<MockEventEngine>();
  auto& mock_event_engine = *mock_event_engine_ptr.get();
  EventEngineExecutor class_under_test(std::move(mock_event_engine_ptr));

  absl::Duration delay = absl::Milliseconds(1);
  EXPECT_CALL(mock_event_engine, RunAfter(ToChronoNanoseconds(delay),
                                          An<absl::AnyInvocable<void()>>()))
      .Times(1)
      .WillOnce(FakeRunAfter);

  absl::AnyInvocable<void()> lambda = []() {};
  TaskId id = class_under_test.RunAfter(delay, std::move(lambda));
  EXPECT_CALL(mock_event_engine, Cancel)
      .Times(1)
      .WillOnce([&](TaskHandle task_handle) -> bool {
        EXPECT_THAT(id.keys, testing::ElementsAre(*task_handle.keys,
                                                  *(task_handle.keys + 1)));
        return true;
      });

  EXPECT_TRUE(class_under_test.Cancel(id));
}

}  // namespace
}  // namespace privacy_sandbox::server_common
