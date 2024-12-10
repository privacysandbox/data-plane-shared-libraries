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

#ifndef UTIL_PERIODIC_CLOSURE_H_
#define UTIL_PERIODIC_CLOSURE_H_

#include <functional>
#include <memory>

#include "absl/functional/any_invocable.h"
#include "absl/status/status.h"
#include "absl/time/time.h"

namespace privacy_sandbox::server_common {

// Runs a closure repeatedly on a thread owned by this class.
// Can only be started once.
// All methods are not thread-safe.
class PeriodicClosure {
 public:
  virtual ~PeriodicClosure() = default;

  // Executes `closure` immediately, then every `interval`.
  virtual absl::Status StartNow(absl::Duration interval,
                                absl::AnyInvocable<void()> closure) = 0;

  // Executes `closure` every `interval`, with no immediate call.
  virtual absl::Status StartDelayed(absl::Duration interval,
                                    absl::AnyInvocable<void()> closure) = 0;

  virtual void Stop() = 0;

  virtual bool IsRunning() const = 0;

  static std::unique_ptr<PeriodicClosure> Create();
};
}  // namespace privacy_sandbox::server_common

#endif  // UTIL_PERIODIC_CLOSURE_H_
