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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_JS_ENGINE_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_JS_ENGINE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "include/libplatform/libplatform.h"
#include "include/v8.h"
#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/js_engine/js_engine.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_function_binding.h"
#include "src/roma/sandbox/js_engine/v8_engine/v8_isolate_wrapper.h"
#include "src/roma/worker/execution_utils.h"
#include "src/roma/worker/execution_watchdog.h"

#include "snapshot_compilation_context.h"
#include "v8_console.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
/**
 * @brief Implementation of a JS engine using v8
 *
 */
class V8JsEngine : public JsEngine {
 public:
  V8JsEngine(std::unique_ptr<V8IsolateFunctionBinding>
                 isolate_function_binding = nullptr,
             bool skip_v8_cleanup = false, bool enable_profilers = false,
             const JsEngineResourceConstraints& v8_resource_constraints =
                 JsEngineResourceConstraints(),
             bool logging_function_set = false,
             bool disable_udf_stacktraces_in_response = false);

  ~V8JsEngine() override;

  void Run() override;

  void Stop() override;

  void OneTimeSetup(
      const absl::flat_hash_map<std::string, std::string>& config =
          absl::flat_hash_map<std::string, std::string>()) override;

  absl::StatusOr<js_engine::JsEngineExecutionResponse> CompileAndRunJs(
      std::string_view code, std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
      const js_engine::RomaJsEngineCompilationContext& context =
          RomaJsEngineCompilationContext()) override;

  absl::StatusOr<js_engine::JsEngineExecutionResponse> CompileAndRunJsWithWasm(
      std::string_view code, absl::Span<const std::uint8_t> wasm,
      std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
      const js_engine::RomaJsEngineCompilationContext& context =
          RomaJsEngineCompilationContext()) override
      ABSL_LOCKS_EXCLUDED(console_mutex_);

  absl::StatusOr<js_engine::JsEngineExecutionResponse> CompileAndRunWasm(
      std::string_view code, std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
      const js_engine::RomaJsEngineCompilationContext& context =
          RomaJsEngineCompilationContext()) override;

 private:
  /**
   * @brief Create a context in given isolate with isolate_function_binding
   * registered.
   *
   * @param isolate
   * @param context
   * @return absl::Status
   */
  absl::Status CreateV8Context(v8::Isolate* isolate,
                               v8::Local<v8::Context>& context);

  /**
   * @brief Create a Snapshot object
   *
   * @param startup_data
   * @param js_code
   * @return absl::Status
   */
  absl::Status CreateSnapshot(v8::StartupData& startup_data,
                              std::string_view js_code);
  /**
   * @brief Create a Snapshot object with start up data containing global
   * objects that can be directly referenced in the JS code.
   *
   * @param startup_data
   * @param wasm
   * @param metadata
   * @return absl::Status
   */
  absl::Status CreateSnapshotWithGlobals(
      v8::StartupData& startup_data, absl::Span<const std::uint8_t> wasm,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata);
  /**
   * @brief Create a Compilation Context object which wraps a object of
   * SnapshotCompilationContext in the context.
   *
   * @param code
   * @param wasm
   * @param metadata
   * @return
   * absl::StatusOr<js_engine::RomaJsEngineCompilationContext>
   */
  absl::StatusOr<js_engine::RomaJsEngineCompilationContext>
  CreateCompilationContext(
      std::string_view code, absl::Span<const std::uint8_t> wasm,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata);

  /// @brief Create a v8 isolate instance.  Returns nullptr on failure.
  virtual std::unique_ptr<V8IsolateWrapper> CreateIsolate(
      const v8::StartupData& startup_data = {nullptr, 0});

  /**
   * @brief Start timing the execution running in the isolate with watchdog.
   *
   * @param isolate the target isolate where the execution is running.
   * @param metadata metadata from the request which may contain a
   * kTimeoutDurationTag with the timeout value. If there is no
   * kTimeoutDurationTag, the default timeout value kDefaultExecutionTimeout
   * will be used.
   */
  void StartWatchdogTimer(
      v8::Isolate* isolate,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata);
  /**
   * @brief Stop the timer for the execution in isolate. Call this function
   * after execution is complete to avoid watchdog termination of standby
   * isolate.
   *
   */
  void StopWatchdogTimer();

  /**
   * @brief Execute invocation request in current compilation context.
   *
   * @param current_compilation_context
   * @param function_name
   * @param input
   * @param metadata
   * @return absl::StatusOr<ExecutionResponse>
   */
  absl::StatusOr<ExecutionResponse> ExecuteJs(
      const std::shared_ptr<SnapshotCompilationContext>&
          current_compilation_context,
      std::string_view function_name,
      const std::vector<std::string_view>& input,
      const absl::flat_hash_map<std::string_view, std::string_view>& metadata);

  /**
   * @brief Compile the wasm code array as a wasm module.
   *
   * @param isolate
   * @param wasm
   * @return absl::Status
   */
  absl::Status CompileWasmCodeArray(v8::Isolate* isolate,
                                    absl::Span<const std::uint8_t> wasm);

  /**
   * @brief Log `msg` using logging function in host process with severity from
   * `function_name`. Per-request metadata needs to be included in `metadata`.
   *
   * @param function_name
   * @param msg
   * @param metadata
   * @return absl::Status
   */
  absl::Status HandleLog(std::string_view function_name, std::string_view msg,
                         google::scp::roma::logging::LogOptions log_options);

  /**
   * @brief Format top_level_error and log stacktrace in host process.
   *
   * @param isolate
   * @param try_catch
   * @param context
   * @param top_level_error
   * @param uuid
   * @param id
   * @param min_log_level
   * @return absl::Status
   */
  absl::Status FormatAndLogError(
      v8::Isolate* isolate, v8::TryCatch& try_catch,
      v8::Local<v8::Context> context, std::string_view top_level_error,
      google::scp::roma::logging::LogOptions log_options);

  std::unique_ptr<V8IsolateWrapper> isolate_wrapper_;
  std::unique_ptr<V8IsolateFunctionBinding> isolate_function_binding_;

  /// @brief These are external references (pointers to data outside of the
  /// v8 heap) which are needed for serialization of the v8 snapshot.
  std::vector<intptr_t> external_references_;

  /// v8 heap resource constraints.
  const JsEngineResourceConstraints v8_resource_constraints_;
  /// @brief A timer thread watches the code execution in v8 isolate and
  /// timeouts the execution in set time.
  std::unique_ptr<roma::worker::ExecutionWatchDog> execution_watchdog_{nullptr};

  V8Console* console() ABSL_LOCKS_EXCLUDED(console_mutex_);
  std::unique_ptr<V8Console> console_ ABSL_GUARDED_BY(console_mutex_);
  absl::Mutex console_mutex_;
  const bool skip_v8_cleanup_;
  const bool enable_profilers_;
  const bool logging_function_set_;
  const bool disable_udf_stacktraces_in_response_;
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_JS_ENGINE_H_
