/*
 * Copyright 2024 Google LLC
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
#include <iostream>

#include "absl/time/time.h"
#include "include/v8.h"
#include "src/util/duration.h"

#include "performance_now.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
namespace {
const privacy_sandbox::server_common::CpuThreadTimeStopwatch kStopwatch;
}

void PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& info) {
  info.GetReturnValue().Set(
      absl::ToDoubleMilliseconds(kStopwatch.GetElapsedTime()));
}

v8::Local<v8::Value> GetPerformanceStartTime(v8::Isolate* isolate) {
  return v8::Number::New(isolate,
                         absl::ToDoubleMilliseconds(kStopwatch.GetStartTime()));
}

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
