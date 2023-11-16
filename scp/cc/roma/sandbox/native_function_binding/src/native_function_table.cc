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

#include "native_function_table.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_ROMA_FUNCTION_TABLE_COULD_NOT_FIND_FUNCTION_NAME;
using google::scp::core::errors::SC_ROMA_FUNCTION_TABLE_NAME_ALREADY_REGISTERED;

namespace google::scp::roma::sandbox::native_function_binding {
ExecutionResult NativeFunctionTable::Register(absl::string_view function_name,
                                              NativeBinding binding) {
  absl::MutexLock lock(&native_functions_map_mutex_);
  const auto [_, was_inserted] =
      native_functions_.insert({std::string(function_name), binding});

  if (!was_inserted) {
    return FailureExecutionResult(
        SC_ROMA_FUNCTION_TABLE_NAME_ALREADY_REGISTERED);
  }
  return SuccessExecutionResult();
}

ExecutionResult NativeFunctionTable::Call(
    absl::string_view function_name,
    FunctionBindingPayload& function_binding_wrapper) {
  NativeBinding func;
  {
    absl::MutexLock lock(&native_functions_map_mutex_);
    auto fn_it = native_functions_.find(function_name);
    if (fn_it == native_functions_.end()) {
      return FailureExecutionResult(
          SC_ROMA_FUNCTION_TABLE_COULD_NOT_FIND_FUNCTION_NAME);
    }
    func = fn_it->second;
  }
  func(function_binding_wrapper);
  return SuccessExecutionResult();
}

void NativeFunctionTable::Clear() {
  absl::MutexLock lock(&native_functions_map_mutex_);
  native_functions_.clear();
}

}  // namespace google::scp::roma::sandbox::native_function_binding
