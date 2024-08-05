/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_PERFORMANCE_NOW_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_PERFORMANCE_NOW_H_

#include "include/v8.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

void PerformanceNow(const v8::FunctionCallbackInfo<v8::Value>& info);

v8::Local<v8::Value> GetPerformanceStartTime(v8::Isolate* isolate);

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_PERFORMANCE_NOW_H_
