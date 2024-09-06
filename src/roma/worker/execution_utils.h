/*
 * Copyright 2022 Google LLC
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

#ifndef ROMA_WORKER_EXECUTION_UTILS_H_
#define ROMA_WORKER_EXECUTION_UTILS_H_

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "include/v8.h"
#include "src/roma/interface/roma.h"

namespace google::scp::roma::worker {

class ExecutionUtils {
 public:
  /**
   * @brief Compiles and runs JavaScript code object.
   *
   * @param js the string object of JavaScript code.
   * @param[out] unbound_script this is optional output. If unbound_script is
   * provided, a local UnboundScript will be assigned to unbound_script.
   * @return absl::Status
   */
  static absl::Status CompileRunJS(
      std::string_view js, bool logging_function_set = false,
      absl::Nullable<v8::Local<v8::UnboundScript>*> unbound_script = nullptr);

  /**
   * @brief Get JS handler from context.
   *
   * @param handler_name the name of the handler.
   * @param handler the handler of the code object.
   * @return absl::Status
   */
  static absl::Status GetJsHandler(std::string_view handler_name,
                                   v8::Local<v8::Value>& handler);

  /**
   * @brief Compiles and runs WASM code object.
   *
   * @param wasm the byte object of WASM code.
   * @return absl::Status the execution result of JavaScript code
   * object compile and run.
   */
  static absl::Status CompileRunWASM(std::string_view wasm);

  /**
   * @brief Get handler from WASM export object.
   *
   * @param handler_name the name of the handler.
   * @param handler the handler of the code object.
   * @return absl::Status
   */
  static absl::Status GetWasmHandler(std::string_view handler_name,
                                     v8::Local<v8::Value>& handler);

  /**
   * @brief Converts string vector to v8 Array.
   *
   * @param input The object of std::vector<std::string>.
   * @param is_wasm Whether this is targeted towards a WASM handler.
   * @param is_byte_str On the JS handler, whether the input string should be
   * processed as a byte string.
   * @return v8::Local<v8::String> The output of v8 Value.
   */
  static v8::Local<v8::Array> InputToLocalArgv(
      const std::vector<std::string_view>& input, bool is_wasm = false,
      bool is_byte_str = false);

  /**
   * @brief Read a value from WASM memory
   *
   * @param isolate
   * @param context
   * @param offset
   * @param read_value_type The type of the WASM value being read
   * @return v8::Local<v8::Value>
   */
  static v8::Local<v8::Value> ReadFromWasmMemory(
      absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Context>& context,
      int32_t offset);

  /**
   * @brief Extract the error message from v8::Message object.
   *
   * @param isolate
   * @param message
   * @return std::string
   */
  static std::string ExtractMessage(absl::Nonnull<v8::Isolate*> isolate,
                                    v8::Local<v8::Message> message);

  /**
   * @brief Parse the input string directly to turn it into a v8::String type.
   *
   * @param input
   * @param is_byte_str Whether the input string should be processed as a byte
   * string.
   * @return Local<Array> The array of parsed values.
   */
  static v8::Local<v8::Array> ParseAsJsInput(
      const std::vector<std::string_view>& input, bool is_byte_str = false);

  /**
   * @brief Parse the handler input to be provided to a WASM handler.
   * This function handles writing to the WASM memory if necessary.
   *
   * @param isolate
   * @param context
   * @param input
   * @return Local<Array> The input arguments to be provided to the WASM
   * handler.
   */
  static v8::Local<v8::Array> ParseAsWasmInput(
      absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Context>& context,
      const std::vector<std::string_view>& input);

  /**
   * @brief Check if err_msg contains a WebAssembly ReferenceError.
   *
   * @param err_msg
   * @return true
   * @return false
   */
  static bool CheckErrorWithWebAssembly(std::string_view err_msg) {
    constexpr std::string_view kJsWasmMixedError =
        "ReferenceError: WebAssembly is not defined";
    return err_msg.find(kJsWasmMixedError) != std::string::npos;
  }

  /**
   * @brief Create an Unbound Script object.
   *
   * @param js
   * @return absl::Status
   */
  static absl::Status CreateUnboundScript(
      v8::Global<v8::UnboundScript>& unbound_script,
      absl::Nonnull<v8::Isolate*> isolate, std::string_view js);

  /**
   * @brief Bind UnboundScript to current context and run it.
   *
   * @return bool success
   */
  static absl::Status BindUnboundScript(
      const v8::Global<v8::UnboundScript>& global_unbound_script);

  /**
   * @brief Generate an object that represents the WASM imports modules
   *
   * @param isolate
   * @return Local<Object>
   */
  static v8::Local<v8::Object> GenerateWasmImports(
      absl::Nonnull<v8::Isolate*> isolate);

  static std::string DescribeError(absl::Nonnull<v8::Isolate*> isolate,
                                   absl::Nonnull<v8::TryCatch*> try_catch);

  /**
   * @brief Get the WASM memory object that was registered in the global context
   *
   * @param isolate
   * @param context
   * @return Local<Value> The WASM memory object
   */
  static v8::Local<v8::Value> GetWasmMemoryObject(
      absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Context>& context);

  static absl::Status V8PromiseHandler(absl::Nonnull<v8::Isolate*> isolate,
                                       v8::Local<v8::Value>& result);

  static absl::Status OverrideConsoleLog(v8::Isolate* isolate,
                                         bool logging_function_set);
};
}  // namespace google::scp::roma::worker

#endif  // ROMA_WORKER_EXECUTION_UTILS_H_
