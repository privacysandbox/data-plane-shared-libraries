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

class V8IsolateWrapper {
 public:
  virtual ~V8IsolateWrapper() = default;

  virtual v8::Isolate* isolate() = 0;
};

// V8 has funky memory management so wrap inside a RAII class.
class V8IsolateWrapperImpl final : public V8IsolateWrapper {
 public:
  V8IsolateWrapperImpl(absl::Nonnull<v8::Isolate*> isolate,
                       std::unique_ptr<v8::ArrayBuffer::Allocator> allocator)
      : V8IsolateWrapper(),
        isolate_(isolate),
        allocator_(std::move(allocator)) {}

  ~V8IsolateWrapperImpl() override {
    // Isolates are only deleted this way and not with Free().
    isolate_->Dispose();
  }

  // Not copyable or moveable.
  V8IsolateWrapperImpl(const V8IsolateWrapperImpl&) = delete;
  V8IsolateWrapperImpl& operator=(const V8IsolateWrapperImpl&) = delete;

  v8::Isolate* isolate() override { return isolate_; }

 private:
  v8::Isolate* isolate_;
  // Each isolate has an allocator that lives with it:
  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator_;
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_WRAPPER_H_
