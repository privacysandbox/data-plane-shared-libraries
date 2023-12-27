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

#include "roma/worker/src/execution_watchdog.h"

#include <gtest/gtest.h>

#include <v8-context.h>
#include <v8-initialization.h>
#include <v8-isolate.h>

#include <linux/limits.h>

#include <string>

#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "include/libplatform/libplatform.h"

namespace google::scp::roma::worker::test {

class ExecutionWatchdogTest : public ::testing::Test {
 protected:
  static void SetUpTestSuite() {
    const int my_pid = getpid();
    const std::string proc_exe_path = absl::StrCat("/proc/", my_pid, "/exe");
    std::string my_path(PATH_MAX, '\0');
    ssize_t sz = readlink(proc_exe_path.c_str(), my_path.data(), PATH_MAX);
    ASSERT_GT(sz, 0);
    v8::V8::InitializeICUDefaultLocation(my_path.c_str());
    v8::V8::InitializeExternalStartupData(my_path.c_str());
    platform_ = v8::platform::NewDefaultPlatform().release();
    v8::V8::InitializePlatform(platform_);
    v8::V8::Initialize();
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

TEST_F(ExecutionWatchdogTest, TerminateOnTimeoutStartTimerAfterRun) {
  watch_dog_.Run();
  constexpr absl::Duration duration = absl::Milliseconds(10);
  watch_dog_.StartTimer(isolate_, duration);
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  absl::SleepFor(absl::Milliseconds(1));
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  absl::SleepFor(duration + absl::Milliseconds(5));
  ASSERT_TRUE(watch_dog_.IsTerminateCalled());
  watch_dog_.Stop();
}

TEST_F(ExecutionWatchdogTest, TerminateOnTimeoutStartTimerBeforeRun) {
  constexpr absl::Duration duration = absl::Milliseconds(10);
  watch_dog_.StartTimer(isolate_, duration);
  watch_dog_.Run();
  absl::SleepFor(absl::Milliseconds(1));
  ASSERT_FALSE(watch_dog_.IsTerminateCalled());
  absl::SleepFor(duration + absl::Milliseconds(5));
  ASSERT_TRUE(watch_dog_.IsTerminateCalled());
  watch_dog_.Stop();
}

}  // namespace google::scp::roma::worker::test
