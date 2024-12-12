// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/util/periodic_closure.h"

#include <chrono>
#include <thread>
#include <utility>

#include "absl/synchronization/notification.h"
#include "absl/time/clock.h"
#include "gtest/gtest.h"

namespace privacy_sandbox::server_common {
namespace {

using privacy_sandbox::server_common::PeriodicClosure;

TEST(PeriodicClosureTest, IsNotRunning) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  ASSERT_FALSE(periodic_closure->IsRunning());
}

TEST(PeriodicClosureTest, IsRunning) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  ASSERT_TRUE(periodic_closure->StartNow(absl::ZeroDuration(), []() {}).ok());
  ASSERT_TRUE(periodic_closure->IsRunning());
}

TEST(PeriodicClosureTest, StartDelayed) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  absl::Notification notification;
  constexpr absl::Duration delay = absl::Milliseconds(2);
  const absl::Time start = absl::Now();
  absl::Time delayed_start;
  absl::AnyInvocable<void()> callback = [&delayed_start, &notification]() {
    if (!notification.HasBeenNotified()) {
      delayed_start = absl::Now();
      notification.Notify();
    }
  };

  ASSERT_TRUE(periodic_closure->StartDelayed(delay, std::move(callback)).ok());
  notification.WaitForNotification();
  ASSERT_GE(delayed_start - start, delay);
}

TEST(PeriodicClosureTest, StartNow) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  absl::Notification notification;
  constexpr absl::Duration delay = absl::Minutes(2);
  const absl::Time start = absl::Now();
  absl::Time delayed_start;
  absl::AnyInvocable<void()> callback = [&delayed_start, &notification]() {
    if (!notification.HasBeenNotified()) {
      delayed_start = absl::Now();
      notification.Notify();
    }
  };

  ASSERT_TRUE(periodic_closure->StartNow(delay, std::move(callback)).ok());
  notification.WaitForNotification();
  ASSERT_LT(delayed_start - start, delay);
}

TEST(PeriodicClosureTest, Stop) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  absl::Notification notification;
  absl::AnyInvocable<void()> callback = [&notification]() {
    if (!notification.HasBeenNotified()) {
      notification.Notify();
    }
  };

  ASSERT_TRUE(
      periodic_closure->StartNow(absl::Milliseconds(1), std::move(callback))
          .ok());
  notification.WaitForNotification();
  periodic_closure->Stop();
  ASSERT_FALSE(periodic_closure->IsRunning());
}

TEST(PeriodicClosureTest, StartAfterStarted) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  absl::Notification notification;
  absl::AnyInvocable<void()> callback = [&]() {
    if (!notification.HasBeenNotified()) {
      notification.Notify();
    }
  };

  ASSERT_TRUE(
      periodic_closure->StartNow(absl::Milliseconds(1), std::move(callback))
          .ok());
  notification.WaitForNotification();
  ASSERT_FALSE(periodic_closure->StartNow(absl::Milliseconds(1), []() {}).ok());
}

TEST(PeriodicClosureTest, StartAfterStopped) {
  std::unique_ptr<PeriodicClosure> periodic_closure = PeriodicClosure::Create();
  ASSERT_TRUE(periodic_closure->StartNow(absl::Milliseconds(1), []() {}).ok());
  periodic_closure->Stop();
  ASSERT_FALSE(periodic_closure->StartNow(absl::Milliseconds(1), []() {}).ok());
}

}  // namespace
}  // namespace privacy_sandbox::server_common
