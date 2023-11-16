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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_TABLE_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_TABLE_H_

#include <functional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "scp/cc/public/core/interface/execution_result.h"
#include "scp/cc/roma/config/src/function_binding_object_v2.h"
#include "scp/cc/roma/interface/function_binding_io.pb.h"

#include "error_codes.h"

namespace google::scp::roma::sandbox::native_function_binding {
class NativeFunctionTable {
 public:
  using NativeBinding =
      std::function<void(FunctionBindingPayload& binding_wrapper)>;

  /**
   * @brief Register a function binding in the table.
   *
   * @param function_name The name of the function.
   * @param binding The actual function.
   * @return core::ExecutionResult
   */
  core::ExecutionResult Register(absl::string_view function_name,
                                 NativeBinding binding)
      ABSL_LOCKS_EXCLUDED(native_functions_map_mutex_);

  /**
   * @brief Call a function that has been previously registered.
   *
   * @param function_name The function name.
   * @param function_binding_proto The function parameters.
   * @return core::ExecutionResult
   */
  core::ExecutionResult Call(absl::string_view function_name,
                             FunctionBindingPayload& function_binding_wrapper)
      ABSL_LOCKS_EXCLUDED(native_functions_map_mutex_);

  // Remove all of the functions from the table.
  void Clear() ABSL_LOCKS_EXCLUDED(native_functions_map_mutex_);

 private:
  absl::flat_hash_map<std::string, NativeBinding> native_functions_
      ABSL_GUARDED_BY(native_functions_map_mutex_);
  absl::Mutex native_functions_map_mutex_;
};
}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_SRC_NATIVE_FUNCTION_TABLE_H_
