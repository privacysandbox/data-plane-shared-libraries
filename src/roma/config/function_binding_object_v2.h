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

#ifndef ROMA_CONFIG_FUNCTION_BINDING_OBJECT_V2_H_
#define ROMA_CONFIG_FUNCTION_BINDING_OBJECT_V2_H_

#include <functional>
#include <string>

#include "absl/container/flat_hash_map.h"
#include "src/roma/interface/function_binding_io.pb.h"

namespace google::scp::roma {

// The default metadata type.
using DefaultMetadata = absl::flat_hash_map<std::string, std::string>;

template <typename TMetadata = DefaultMetadata>
struct FunctionBindingPayload {
  /**
   * @brief The two-way proto used to receive input from the JS function,
   * and return output from the C++ function
   */
  proto::FunctionBindingIoProto& io_proto;

  /**
   * @brief Metadata passed in from InvocationRequest to native functions
   * outside of sandbox.
   */
  const TMetadata& metadata;
};

template <typename TMetadata = DefaultMetadata>
class FunctionBindingObjectV2 {
 public:
  /**
   * @brief The name by which Javascript code can call this function.
   */
  std::string function_name;

  /**
   * @brief The function that will be bound to a Javascript function.
   */
  std::function<void(FunctionBindingPayload<TMetadata>&)> function;
};

}  // namespace google::scp::roma

#endif  // ROMA_CONFIG_FUNCTION_BINDING_OBJECT_V2_H_
