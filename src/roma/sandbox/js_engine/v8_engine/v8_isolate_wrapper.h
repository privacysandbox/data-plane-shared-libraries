/*
 * Copyright 2023 Google LLC
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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_

#include <memory>
#include <utility>

#include "include/v8.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

// V8 has funky memory management so wrap inside a RAII class.
class V8IsolateWrapper final {
 public:
  V8IsolateWrapper(absl::Nonnull<v8::Isolate*> isolate,
                   std::unique_ptr<v8::ArrayBuffer::Allocator> allocator)
      : isolate_(isolate), allocator_(std::move(allocator)) {}

  ~V8IsolateWrapper() {
    // Isolates are only deleted this way and not with Free().
    isolate_->Dispose();
  }

  // Not copyable or moveable.
  V8IsolateWrapper(const V8IsolateWrapper&) = delete;
  V8IsolateWrapper& operator=(const V8IsolateWrapper&) = delete;

  v8::Isolate* isolate() { return isolate_; }

 private:
  v8::Isolate* isolate_;
  // Each isolate has an allocator that lives with it:
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
};

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_
