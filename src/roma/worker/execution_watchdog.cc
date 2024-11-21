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

#include "execution_watchdog.h"

#include <thread>

#include "absl/synchronization/blocking_counter.h"
#include "absl/time/time.h"
#include "include/v8.h"

namespace google::scp::roma::worker {

ExecutionWatchDog::ExecutionWatchDog()
    : expiring_flag_(SteadyClock::RealClock()),
      is_running_(false),
      is_terminate_called_(false) {
  expiring_flag_.Set(absl::InfiniteDuration());
}

ExecutionWatchDog::~ExecutionWatchDog() { Stop(); }

void ExecutionWatchDog::Run() {
  absl::MutexLock lock(&mutex_);
  execution_watchdog_thread_ =
      std::thread(&ExecutionWatchDog::WaitForTimeout, this);
  mutex_.Await(absl::Condition(+[](bool* b) { return *b; }, &is_running_));
}

void ExecutionWatchDog::Stop() {
  {
    absl::MutexLock lock(&mutex_);
    if (!is_running_) {
      return;
    }
    is_running_ = false;
    cv_.Signal();
  }
  if (execution_watchdog_thread_.joinable()) {
    execution_watchdog_thread_.join();
  }
}

bool ExecutionWatchDog::IsTerminateCalled() {
  absl::MutexLock lock(&mutex_);
  return is_terminate_called_;
}

void ExecutionWatchDog::StartTimer(absl::Nonnull<v8::Isolate*> isolate,
                                   absl::Duration timeout) {
  absl::MutexLock lock(&mutex_);
  // Cancel TerminateExecution in case there was a previous
  // isolate->TerminateExecution() flag alive
  isolate->CancelTerminateExecution();
  v8_isolate_ = isolate;
  expiring_flag_.Set(timeout);
  is_terminate_called_ = false;
  cv_.Signal();
}

void ExecutionWatchDog::EndTimer() {
  absl::MutexLock lock(&mutex_);
  expiring_flag_.Set(absl::InfiniteDuration());
}

void ExecutionWatchDog::WaitForTimeout() {
  absl::MutexLock lock(&mutex_);
  is_running_ = true;
  while (is_running_) {
    if (!expiring_flag_.Get()) {
      v8_isolate_->TerminateExecution();
      is_terminate_called_ = true;
      expiring_flag_.Set(absl::InfiniteDuration());
    }
    cv_.WaitWithTimeout(&mutex_, expiring_flag_.GetTimeRemaining());
  }
}

}  // namespace google::scp::roma::worker
