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

#include "src/roma/worker/execution_watchdog.h"

#include <gtest/gtest.h>

#include <string>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "src/util/process_util.h"

namespace google::scp::roma::worker::test {

class ExecutionWatchdogTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    absl::StatusOr<std::string> my_path =
        ::privacy_sandbox::server_common::GetExePath();
    CHECK_OK(my_path);
    v8::V8::InitializeICUDefaultLocation(my_path->data());
    v8::V8::InitializeExternalStartupData(my_path->data());
    platform_ = v8::platform::NewDefaultPlatform().release();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
  }

  static void TearDownTestSuite() {
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
  }

  void SetUp() override {
    create_params_.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params_);
  }

  void TearDown() override {
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  v8::Isolate::CreateParams create_params_;
  static v8::Platform* platform_;
  v8::Isolate* isolate_{nullptr};
  ExecutionWatchDog watch_dog_;
};

v8::Platform* ExecutionWatchdogTest::platform_{nullptr};

TEST_F(ExecutionWatchdogTest, StopDoesntTerminate) {
  watch_dog_.Run();
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  watch_dog_.Stop();
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
}

// TODO: b/309509915 - Enable test once issue is solved.
TEST_F(ExecutionWatchdogTest, DISABLED_TerminateOnTimeoutStartTimerAfterRun) {
  watch_dog_.Run();
  constexpr absl::Duration duration = absl::Milliseconds(10);
  watch_dog_.StartTimer(isolate_, duration);
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  absl::SleepFor(duration / 2);
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  absl::SleepFor(duration / 2 + absl::Milliseconds(1));
  ASSERT_TRUE(watch_dog_.IsTerminateCalled());
  watch_dog_.Stop();
}

// TODO: b/309509915 - Enable test once issue is solved.
TEST_F(ExecutionWatchdogTest, DISABLED_TerminateOnTimeoutStartTimerBeforeRun) {
  constexpr absl::Duration duration = absl::Milliseconds(10);
  watch_dog_.StartTimer(isolate_, duration);
  watch_dog_.Run();
  absl::SleepFor(duration / 2);
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  absl::SleepFor(duration / 2 + absl::Milliseconds(1));
  ASSERT_TRUE(watch_dog_.IsTerminateCalled());
  watch_dog_.Stop();
}

}  // namespace google::scp::roma::worker::test
