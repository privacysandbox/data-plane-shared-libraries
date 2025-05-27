//  Copyright 2025 Google LLC
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

#include "src/core/event/event_base.h"

#include "event2/event.h"
#include "event2/thread.h"

namespace privacy_sandbox::server_common {

EventBase::EventBase(bool use_pthreads, int num_priorities) {
  if (use_pthreads) {
    evthread_use_pthreads();
  }
  event_base_ = event_base_new();
  event_base_priority_init(event_base_, num_priorities);
  if (server_common::log::PS_VLOG_IS_ON(10)) {
    event_enable_debug_mode();
  }
}

EventBase::~EventBase() {
  if (event_base_ != nullptr) {
    event_base_free(event_base_);
  }
}

struct event_base* EventBase::get() { return event_base_; }

}  // namespace privacy_sandbox::server_common
