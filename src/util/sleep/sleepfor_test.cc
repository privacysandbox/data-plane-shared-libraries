// Copyright 2024 Google LLC
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

#include "src/util/sleep/sleepfor.h"

#include <thread>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "gtest/gtest.h"

namespace privacy_sandbox::server_common {
namespace {

TEST(SleepForTest, DoesSleep) {
  SleepFor sleep_for;
  absl::Time start = absl::Now();
  sleep_for.Duration(absl::Seconds(2));
  absl::Duration total = absl::Now() - start;
  // Make sure it is close enough to intended sleep time.
  ASSERT_LT(total - absl::Seconds(2), absl::Milliseconds(20));
}

TEST(SleepForTest, DoesConcurrentSleep) {
  SleepFor sleep_for;
  absl::Duration d1;
  std::thread t1([&sleep_for, &d1]() {
    const auto start = absl::Now();
    sleep_for.Duration(absl::Seconds(2));
    d1 = absl::Now() - start;
  });
  absl::Duration d2;
  std::thread t2([&sleep_for, &d2]() {
    const auto start = absl::Now();
    sleep_for.Duration(absl::Seconds(2));
    d2 = absl::Now() - start;
  });
  t1.join();
  t2.join();
  // Make sure it is close enough to intended sleep time.
  ASSERT_LT(d1 - absl::Seconds(2), absl::Milliseconds(20));
  ASSERT_LT(d2 - absl::Seconds(2), absl::Milliseconds(20));
}

TEST(SleepForTest, DoesSleepStop) {
  SleepFor sleep_for;
  absl::Time start;
  std::thread t([&sleep_for, &start]() {
    start = absl::Now();
    sleep_for.Duration(absl::Minutes(10));
  });
  auto status = sleep_for.Stop();
  ASSERT_TRUE(status.ok()) << status;
  t.join();
  absl::Duration total = absl::Now() - start;
  // Make sure stop is fast enough
  ASSERT_LT(total, absl::Seconds(20));
  // Can't stop twice
  status = sleep_for.Stop();
  ASSERT_FALSE(status.ok()) << status;
}

}  // namespace
}  // namespace privacy_sandbox::server_common
