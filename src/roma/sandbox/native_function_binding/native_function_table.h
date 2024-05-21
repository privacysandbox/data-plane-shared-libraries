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

#ifndef ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_TABLE_H_
#define ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_TABLE_H_

#include <functional>
#include <string>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "src/roma/config/config.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/roma/interface/function_binding_io.pb.h"

namespace google::scp::roma::sandbox::native_function_binding {

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class NativeFunctionTable {
 public:
  using NativeBinding =
      std::function<void(FunctionBindingPayload<TMetadata>& binding_wrapper)>;

  /**
   * @brief Register a function binding in the table.
   *
   * @param function_name The name of the function.
   * @param binding The actual function.
   * @return absl::Status
   */
  absl::Status Register(std::string_view function_name, NativeBinding binding)
      ABSL_LOCKS_EXCLUDED(native_functions_map_mutex_) {
    absl::MutexLock lock(&native_functions_map_mutex_);
    const auto [_, was_inserted] =
        native_functions_.insert({std::string(function_name), binding});

    if (!was_inserted) {
      return absl::InvalidArgumentError(
          "A function with this name has already been registered in the "
          "table.");
    }
    return absl::OkStatus();
  }

  /**
   * @brief Call a function that has been previously registered.
   *
   * @param function_name The function name.
   * @param function_binding_proto The function parameters.
   * @return absl::Status
   */
  absl::Status Call(std::string_view function_name,
                    FunctionBindingPayload<TMetadata>& function_binding_wrapper)
      ABSL_LOCKS_EXCLUDED(native_functions_map_mutex_) {
    NativeBinding func;
    {
      absl::MutexLock lock(&native_functions_map_mutex_);
      auto fn_it = native_functions_.find(function_name);
      if (fn_it == native_functions_.end()) {
        return absl::InvalidArgumentError(
            absl::StrCat("Could not find the function by name in the table: ",
                         function_name));
      }
      func = fn_it->second;
    }
    func(function_binding_wrapper);
    return absl::OkStatus();
  }

  // Remove all of the functions from the table.
  void Clear() ABSL_LOCKS_EXCLUDED(native_functions_map_mutex_) {
    absl::MutexLock lock(&native_functions_map_mutex_);
    native_functions_.clear();
  }

 private:
  absl::flat_hash_map<std::string, NativeBinding> native_functions_
      ABSL_GUARDED_BY(native_functions_map_mutex_);
  absl::Mutex native_functions_map_mutex_;
};

}  // namespace google::scp::roma::sandbox::native_function_binding

#endif  // ROMA_SANDBOX_NATIVE_FUNCTION_BINDING_NATIVE_FUNCTION_TABLE_H_
