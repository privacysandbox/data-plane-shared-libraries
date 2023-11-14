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

#ifndef ROMA_SANDBOX_JS_ENGINE_SRC_V8_ENGINE_V8_ISOLATE_FUNCTION_BINDING_H_
#define ROMA_SANDBOX_JS_ENGINE_SRC_V8_ENGINE_V8_ISOLATE_FUNCTION_BINDING_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "include/v8.h"
#include "roma/sandbox/native_function_binding/src/native_function_invoker.h"

#include "error_codes.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
class V8IsolateFunctionBinding {
 public:
  /**
   * @brief Create a V8IsolateFunctionBinding instance
   * @param function_names is a list of the names of the functions that can be
   * registered in the v8 context.
   */
  V8IsolateFunctionBinding(
      const std::vector<std::string>& function_names,
      std::unique_ptr<native_function_binding::NativeFunctionInvoker>
          function_invoker)
      : function_invoker_(std::move(function_invoker)) {
    for (const auto& function_name : function_names) {
      binding_references_.emplace_back(std::make_pair(function_name, this));
    }
  }

  // Not copyable or movable
  V8IsolateFunctionBinding(const V8IsolateFunctionBinding&) = delete;
  V8IsolateFunctionBinding& operator=(const V8IsolateFunctionBinding&) = delete;

  core::ExecutionResult BindFunctions(
      v8::Isolate* isolate,
      v8::Local<v8::ObjectTemplate>& global_object_template) noexcept;

  void AddExternalReferences(
      std::vector<intptr_t>& external_references) noexcept;

 private:
  using BindingPair = std::pair<std::string, V8IsolateFunctionBinding*>;
  std::vector<BindingPair> binding_references_;

  static void GlobalV8FunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  std::unique_ptr<native_function_binding::NativeFunctionInvoker>
      function_invoker_;
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_SRC_V8_ENGINE_V8_ISOLATE_FUNCTION_BINDING_H_
