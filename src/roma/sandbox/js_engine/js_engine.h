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

#ifndef ROMA_SANDBOX_JS_ENGINE_JS_ENGINE_H_
#define ROMA_SANDBOX_JS_ENGINE_JS_ENGINE_H_

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "absl/types/span.h"

namespace google::scp::roma::sandbox::js_engine {
/**
 * @brief Context that can be used to load contexts to the JS engine.
 *
 */
struct RomaJsEngineCompilationContext {
  operator bool() const { return bool(context); }

  std::shared_ptr<void> context;
};

struct ExecutionResponse {
  /// the response of handler function execution.
  std::string response;

  /// the output from V8's Heap and Sample-based CPU profiler
  std::string profiler_output;

  /// the metrics for handler function execution.
  absl::flat_hash_map<std::string, absl::Duration> metrics;
};

/**
 * @brief The response returned by the JS engine on a code run.
 *
 */
struct JsEngineExecutionResponse {
  /**
   * The compilation context that was generated when compiling the code
   */
  RomaJsEngineCompilationContext compilation_context;

  /**
   * The response of the JS/WASM execution
   */
  ExecutionResponse execution_response;
};

/**
 * @brief Interface for a JS engine.
 *
 */
class JsEngine {
 public:
  // Destructor must be virtual to avoid memory leaks.
  virtual ~JsEngine() = default;

  virtual void Run() = 0;
  virtual void Stop() = 0;

  /**
   * Function that is intended to be called one at the beginning for any
   * one-time setup that is needed.
   */
  virtual void OneTimeSetup(
      const absl::flat_hash_map<std::string, std::string>& config) = 0;

  /**
   * @brief Builds and runs the JS code provided as input, and returns a context
   * which could be used to avoid recompilation. The context itself is optional.
   * @param code The code to compile and run (optional if context is valid)
   * @param function_name The name of the JS function to invoke
   * @param input The input to pass to the code
   * @param metadata The metadata associated with the code request.
   * @param context A context which could be used to avoid recompiling the JS
   * code
   * @return Whether the operation succeeded or failed, and a result object
   * which contains the response from the JS code and a compilation context
   * which could be used to skip compilation of the same code.
   */
  virtual absl::StatusOr<JsEngineExecutionResponse> CompileAndRunJs(
      std::string_view code, std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
      const RomaJsEngineCompilationContext& context) = 0;

  /**
   * @brief Builds and runs the WASM binary provided as input, and returns a
   * context which could be used to avoid recompilation. The context itself is
   * optional.
   * @param code The code to compile and run (optional if context is valid)
   * @param function_name The name of the WASM function to invoke
   * @param input The input to pass to the code
   * @param metadata The metadata associated with the code request.
   * @param context A context which could be used to avoid recompiling the JS
   * code
   * @return Whether the operation succeeded or failed, and a result object
   * which contains the response from the WASM code and a compilation context
   * which could be used to skip compilation of the same code.
   */
  virtual absl::StatusOr<JsEngineExecutionResponse> CompileAndRunWasm(
      std::string_view code, std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
      const RomaJsEngineCompilationContext& context) = 0;

  /**
   * @brief Builds and runs the JS code provided as input, loads the provided
   * WASM in the global namespace so that it can be instantiated from the JS
   * code, and returns a context which could be used to avoid recompilation. The
   * context itself is optional.
   * @param code The code to compile and run (optional if context is valid)
   * @param wasm The wasm module needed to be loaded by the JS code (optional if
   * context is valid).
   * @param function_name The name of the JS function to invoke
   * @param input The input to pass to the code
   * @param metadata The metadata associated with the code request.
   * @param context A context which could be used to avoid recompiling the JS
   * code
   * @return Whether the operation succeeded or failed, and a result object
   * which contains the response from the JS code and a compilation context
   * which could be used to skip compilation of the same code.
   */
  virtual absl::StatusOr<JsEngineExecutionResponse> CompileAndRunJsWithWasm(
      std::string_view code, absl::Span<const std::uint8_t> wasm,
      std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
      const RomaJsEngineCompilationContext& context) = 0;
};
}  // namespace google::scp::roma::sandbox::js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_JS_ENGINE_H_
