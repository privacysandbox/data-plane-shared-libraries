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

#include "v8_js_engine.h"

#include <errno.h>
#include <string.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/strings/numbers.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "public/core/interface/execution_result.h"
#include "roma/config/src/type_converter.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "roma/sandbox/worker/src/worker_utils.h"
#include "roma/worker/src/execution_utils.h"
#include "src/cpp/util/duration.h"

#include "error_codes.h"
#include "snapshot_compilation_context.h"

using google::scp::core::ExecutionResult;
using google::scp::core::ExecutionResultOr;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::GetErrorMessage;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_JSON;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_FIND_HANDLER_BY_NAME;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_CREATE_COMPILATION_CONTEXT_FAILED_WITH_EMPTY_CODE;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT;
using google::scp::core::errors::SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED;
using google::scp::roma::kDefaultExecutionTimeout;
using google::scp::roma::kWasmCodeArrayName;
using google::scp::roma::TypeConverter;
using google::scp::roma::sandbox::constants::kHandlerCallMetricJsEngineDuration;
using google::scp::roma::sandbox::constants::
    kInputParsingMetricJsEngineDuration;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::constants::kMaxNumberOfWasm32BitMemPages;
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;
using google::scp::roma::sandbox::constants::kWasmMemPagesV8PlatformFlag;
using google::scp::roma::sandbox::js_engine::JsEngineExecutionResponse;
using google::scp::roma::sandbox::js_engine::RomaJsEngineCompilationContext;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateFunctionBinding;
using google::scp::roma::sandbox::worker::WorkerUtils;
using google::scp::roma::worker::ExecutionUtils;

namespace {

std::shared_ptr<std::string> GetCodeFromContext(
    const RomaJsEngineCompilationContext& context) {
  std::shared_ptr<std::string> code;
  if (context) {
    code = std::static_pointer_cast<std::string>(context.context);
  }
  return code;
}

std::string BuildErrorString(std::vector<std::string> errors) {
  std::string err_str;
  for (const auto& e : errors) {
    absl::StrAppend(&err_str, "\n", e);
  }
  return err_str;
}

std::vector<std::string> GetErrors(v8::Isolate* isolate,
                                   v8::TryCatch& try_catch,
                                   const uint64_t error_code) noexcept {
  std::vector<std::string> errors;
  errors.push_back(std::string(GetErrorMessage(error_code)));
  if (try_catch.HasCaught()) {
    if (std::string error_msg;
        !try_catch.Message().IsEmpty() &&
        TypeConverter<std::string>::FromV8(isolate, try_catch.Message()->Get(),
                                           &error_msg)) {
      errors.push_back(std::move(error_msg));
    }
  }
  return errors;
}

ExecutionResult GetError(v8::Isolate* isolate, v8::TryCatch& try_catch,
                         const uint64_t error_code) noexcept {
  // Caught error message from V8 sandbox only shows in DEBUG mode.
  DLOG(ERROR) << BuildErrorString(GetErrors(isolate, try_catch, error_code));
  return FailureExecutionResult(error_code);
}

}  // namespace

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

ExecutionResult V8JsEngine::Run() noexcept {
  return execution_watchdog_->Run();
}

ExecutionResult V8JsEngine::Stop() noexcept {
  if (execution_watchdog_) {
    execution_watchdog_->Stop();
  }
  DisposeIsolate();
  return SuccessExecutionResult();
}

namespace {
std::string GetExePath() {
  const pid_t my_pid = getpid();
  const auto proc_exe_path = absl::StrCat("/proc/", my_pid, "/exe");
  std::string my_path;
  my_path.reserve(PATH_MAX);
  if (readlink(proc_exe_path.c_str(), my_path.data(), PATH_MAX) < 0) {
    LOG(ERROR) << "Unable to resolve prod pid exe path: " << strerror(errno);
  }
  return my_path;
}
}  // namespace

ExecutionResult V8JsEngine::OneTimeSetup(
    const absl::flat_hash_map<std::string, std::string>& config) noexcept {
  size_t max_wasm_memory_number_of_pages = 0;
  if (const auto it = config.find(kJsEngineOneTimeSetupWasmPagesKey);
      it != config.end()) {
    std::stringstream page_count_converter;
    page_count_converter << it->second;
    page_count_converter >> max_wasm_memory_number_of_pages;
  }

  const std::string my_path = GetExePath();
  v8::V8::InitializeICUDefaultLocation(my_path.data());
  v8::V8::InitializeExternalStartupData(my_path.data());

  // Set the max number of WASM memory pages
  if (max_wasm_memory_number_of_pages != 0) {
    const size_t page_count = std::min(max_wasm_memory_number_of_pages,
                                       kMaxNumberOfWasm32BitMemPages);
    const auto flag_value =
        absl::StrCat(kWasmMemPagesV8PlatformFlag, page_count);
    v8::V8::SetFlagsFromString(flag_value.c_str());
  }

  static v8::Platform* v8_platform = [] {
    std::unique_ptr<v8::Platform> v8_platform =
        v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(v8_platform.get());
    v8::V8::Initialize();
    return v8_platform.release();
  }();

  return SuccessExecutionResult();
}

core::ExecutionResult V8JsEngine::CreateSnapshot(
    v8::StartupData& startup_data, std::string_view js_code,
    std::string& err_msg) noexcept {
  v8::SnapshotCreator creator(external_references_.data());
  v8::Isolate* isolate = creator.GetIsolate();

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context;
    auto execution_result = CreateV8Context(isolate, context);
    RETURN_IF_FAILURE(execution_result);

    v8::Context::Scope context_scope(context);
    //  Compile and run JavaScript code object.
    execution_result = ExecutionUtils::CompileRunJS(js_code, err_msg);
    RETURN_IF_FAILURE(execution_result);

    // Set above context with compiled and run code as the default context for
    // the StartupData blob to create.
    creator.SetDefaultContext(context);
  }
  startup_data =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  return core::SuccessExecutionResult();
}

core::ExecutionResult V8JsEngine::CreateSnapshotWithGlobals(
    v8::StartupData& startup_data, absl::Span<const uint8_t> wasm,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,

    std::string& err_msg) noexcept {
  v8::SnapshotCreator creator(external_references_.data());
  v8::Isolate* isolate = creator.GetIsolate();

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context;
    auto execution_result = CreateV8Context(isolate, context);
    RETURN_IF_FAILURE(execution_result);

    v8::Context::Scope context_scope(context);
    auto wasm_code_array_name_or =
        worker::WorkerUtils::GetValueFromMetadata(metadata, kWasmCodeArrayName);
    if (!wasm_code_array_name_or.result().Successful()) {
      LOG(ERROR)
          << std::string("Get wasm code array name from metadata with error ")
          << GetErrorMessage(wasm_code_array_name_or.result().status_code);
      return wasm_code_array_name_or.result();
    }

    v8::Local<v8::String> name;
    name = TypeConverter<std::string>::ToV8(isolate,
                                            wasm_code_array_name_or.value())
               .As<v8::String>();

    context->Global()->Set(
        context, name,
        TypeConverter<uint8_t*>::ToV8(isolate, wasm.data(), wasm.size()));
    // Set above context with compiled and run code as the default context for
    // the StartupData blob to create.
    creator.SetDefaultContext(context);
  }
  startup_data =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  return core::SuccessExecutionResult();
}

static size_t NearHeapLimitCallback(void* data, size_t current_heap_limit,
                                    size_t initial_heap_limit) {
  LOG(ERROR) << "OOM in JS execution, exiting...";
  return 0;
}

std::unique_ptr<V8IsolateWrapper> V8JsEngine::CreateIsolate(
    const v8::StartupData& startup_data) noexcept {
  v8::Isolate::CreateParams params;

  // Configure v8 resource constraints if initial_heap_size_in_mb or
  // maximum_heap_size_in_mb is nonzero.
  if (v8_resource_constraints_.initial_heap_size_in_mb > 0 ||
      v8_resource_constraints_.maximum_heap_size_in_mb > 0) {
    params.constraints.ConfigureDefaultsFromHeapSize(
        v8_resource_constraints_.initial_heap_size_in_mb * kMB,
        v8_resource_constraints_.maximum_heap_size_in_mb * kMB);
  }

  std::unique_ptr<v8::ArrayBuffer::Allocator> allocator(
      v8::ArrayBuffer::Allocator::NewDefaultAllocator());
  params.external_references = external_references_.data();
  params.array_buffer_allocator = allocator.get();

  // Configure create_params with startup_data if startup_data is
  // available.
  if (startup_data.raw_size > 0 && startup_data.data != nullptr) {
    params.snapshot_blob = &startup_data;
  }

  auto isolate = v8::Isolate::New(params);

  if (!isolate) {
    return nullptr;
  }

  isolate->AddNearHeapLimitCallback(NearHeapLimitCallback, nullptr);

  return std::make_unique<V8IsolateWrapper>(isolate, std::move(allocator));
}

void V8JsEngine::DisposeIsolate() noexcept {
  isolate_wrapper_ = nullptr;
}

void V8JsEngine::StartWatchdogTimer(
    v8::Isolate* isolate,
    const absl::flat_hash_map<std::string_view, std::string_view>&
        metadata) noexcept {
  // Get the timeout value from metadata. If no timeout tag is set, the
  // default value kDefaultExecutionTimeout will be used.
  auto timeout_ms = kDefaultExecutionTimeout;
  auto timeout_str_or =
      WorkerUtils::GetValueFromMetadata(metadata, kTimeoutDurationTag);
  if (timeout_str_or.result().Successful()) {
    if (absl::Duration t; absl::ParseDuration(timeout_str_or.value(), &t)) {
      timeout_ms = t;
    } else {
      LOG(ERROR) << "Timeout tag parsing with error: Could not convert timeout "
                    "tag to absl::Duration.";
    }
  }
  ROMA_VLOG(1) << "StartWatchdogTimer timeout set to " << timeout_ms << " ms";
  execution_watchdog_->StartTimer(isolate, timeout_ms);
}

void V8JsEngine::StopWatchdogTimer() noexcept {
  execution_watchdog_->EndTimer();
}

ExecutionResultOr<RomaJsEngineCompilationContext>
V8JsEngine::CreateCompilationContext(
    std::string_view code, absl::Span<const uint8_t> wasm,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    std::string& err_msg) noexcept {
  if (code.empty()) {
    return FailureExecutionResult(
        SC_ROMA_V8_ENGINE_CREATE_COMPILATION_CONTEXT_FAILED_WITH_EMPTY_CODE);
  }

  auto snapshot_context = std::make_shared<SnapshotCompilationContext>();
  // If wasm code array exists, a snapshot with global wasm code array will be
  // created. Otherwise, a normal snapshot containing compiled JS code will be
  // created.
  bool js_with_wasm = !wasm.empty();
  auto execution_result =
      js_with_wasm
          ? CreateSnapshotWithGlobals(snapshot_context->startup_data, wasm,
                                      metadata, err_msg)
          : CreateSnapshot(snapshot_context->startup_data, code, err_msg);
  std::unique_ptr<V8IsolateWrapper> isolate_or;
  if (execution_result.Successful()) {
    isolate_or = CreateIsolate(snapshot_context->startup_data);
    if (!isolate_or) {
      return FailureExecutionResult(SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE);
    }
    snapshot_context->cache_type = CacheType::kSnapshot;

    if (js_with_wasm) {
      auto wasm_compile_result =
          CompileWasmCodeArray(isolate_or->isolate(), wasm, err_msg);
      if (!wasm_compile_result.Successful()) {
        LOG(ERROR) << "Compile wasm module failed with "
                   << GetErrorMessage(wasm_compile_result.status_code);
        DLOG(ERROR) << "Compile wasm module failed with debug error" << err_msg;
        return wasm_compile_result;
      }
      execution_result = ExecutionUtils::CreateUnboundScript(
          snapshot_context->unbound_script, isolate_or->isolate(), code,
          err_msg);
      if (!execution_result.Successful()) {
        LOG(ERROR) << "CreateUnboundScript failed with "
                   << GetErrorMessage(execution_result.status_code);
        DLOG(ERROR) << "CreateUnboundScript failed with debug errors "
                    << err_msg;
        return execution_result;
      }
      snapshot_context->cache_type = CacheType::kUnboundScript;
    }

    ROMA_VLOG(2) << "compilation context cache type is V8 snapshot";
  } else {
    LOG(ERROR) << "CreateSnapshot failed with "
               << GetErrorMessage(execution_result.status_code);
    // err_msg may contain confidential message which only shows in DEBUG mode.
    DLOG(ERROR) << "CreateSnapshot failed with debug errors " << err_msg;
    // Return the failure if it isn't caused by global WebAssembly.
    if (!ExecutionUtils::CheckErrorWithWebAssembly(err_msg)) {
      return execution_result;
    }

    isolate_or = CreateIsolate();
    if (!isolate_or) {
      return FailureExecutionResult(SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE);
    }

    // TODO(b/298062607): deprecate err_msg, all exceptions should being caught
    // by GetError().
    execution_result = ExecutionUtils::CreateUnboundScript(
        snapshot_context->unbound_script, isolate_or->isolate(), code, err_msg);
    if (!execution_result.Successful()) {
      LOG(ERROR) << "CreateUnboundScript failed with "
                 << GetErrorMessage(execution_result.status_code);
      // err_msg may contain confidential message which only shows in DEBUG
      // mode.
      DLOG(ERROR) << "CreateUnboundScript failed with debug errors " << err_msg;
      return execution_result;
    }

    snapshot_context->cache_type = CacheType::kUnboundScript;
    ROMA_VLOG(2) << "compilation context cache type is V8 UnboundScript";
  }

  // Snapshot the isolate with compilation context and also initialize a
  // execution watchdog inside the isolate.
  snapshot_context->isolate = std::move(isolate_or);

  RomaJsEngineCompilationContext js_ctx{
      .context = snapshot_context,
  };
  return js_ctx;
}

core::ExecutionResult V8JsEngine::CompileWasmCodeArray(
    v8::Isolate* isolate, absl::Span<const uint8_t> wasm,
    std::string& err_msg) noexcept {
  v8::Isolate::Scope isolate_scope(isolate);
  // Create a handle scope to keep the temporary object references.
  v8::HandleScope handle_scope(isolate);
  // Set up an exception handler before calling the Process function
  v8::TryCatch try_catch(isolate);

  v8::Local<v8::Context> v8_context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(v8_context);

  // Check whether wasm module can compile
  auto module_maybe = v8::WasmModuleObject::Compile(
      isolate,
      v8::MemorySpan<const uint8_t>(
          reinterpret_cast<const unsigned char*>(wasm.data()), wasm.size()));
  if (module_maybe.IsEmpty()) {
    return FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE);
  }
  return SuccessExecutionResult();
}

core::ExecutionResultOr<ExecutionResponse> V8JsEngine::ExecuteJs(
    const std::shared_ptr<SnapshotCompilationContext>&
        current_compilation_context,
    std::string_view function_name, const std::vector<absl::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>&
        metadata) noexcept {
  ExecutionResponse execution_response;

  v8::Isolate* v8_isolate = current_compilation_context->isolate->isolate();
  v8::Isolate::Scope isolate_scope(v8_isolate);
  // Create a handle scope to keep the temporary object references.
  v8::HandleScope handle_scope(v8_isolate);
  // Set up an exception handler before calling the Process function
  v8::TryCatch try_catch(v8_isolate);

  v8::Local<v8::Context> v8_context = v8::Context::New(v8_isolate);
  v8::Context::Scope context_scope(v8_context);

  std::string err_msg;
  // Binding UnboundScript to current context when the compilation context is
  // kUnboundScript.
  if (current_compilation_context->cache_type == CacheType::kUnboundScript) {
    auto result = ExecutionUtils::BindUnboundScript(
        current_compilation_context->unbound_script, err_msg);
    if (!result.Successful()) {
      LOG(ERROR) << "BindUnboundScript failed with "
                 << GetErrorMessage(result.status_code);
      DLOG(ERROR) << "BindUnboundScript failed with debug errors " << err_msg;
      return result;
    }
  }

  v8::Local<v8::Value> handler;
  const auto result =
      ExecutionUtils::GetJsHandler(function_name, handler, err_msg);
  if (!result.Successful()) {
    LOG(ERROR) << "GetJsHandler failed with "
               << GetErrorMessage(result.status_code);
    DLOG(ERROR) << "GetJsHandler failed with debug errors " << err_msg;
    return result;
  }

  privacy_sandbox::server_common::Stopwatch stopwatch;
  {
    v8::Local<v8::Function> handler_func = handler.As<v8::Function>();
    stopwatch.Reset();

    const size_t argc = input.size();
    v8::Local<v8::Array> argv_array;

    auto input_type =
        metadata.find(google::scp::roma::sandbox::constants::kInputType);
    const bool uses_input_type = (input_type != metadata.end());
    const bool uses_input_type_bytes =
        (uses_input_type &&
         input_type->second ==
             google::scp::roma::sandbox::constants::kInputTypeBytes);

    argv_array = ExecutionUtils::ParseAsJsInput(input, uses_input_type_bytes);

    // If argv_array size doesn't match with input. Input conversion failed.
    if (argv_array.IsEmpty() || argv_array->Length() != argc) {
      LOG(ERROR) << "Could not parse the inputs";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT);
    }
    v8::Local<v8::Value> argv[argc];
    for (size_t i = 0; i < argc; ++i) {
      argv[i] = argv_array->Get(v8_context, i).ToLocalChecked();
    }
    execution_response.metrics[kInputParsingMetricJsEngineDuration] =
        stopwatch.GetElapsedTime();

    stopwatch.Reset();
    v8::Local<v8::Value> result;
    if (!handler_func->Call(v8_context, v8_context->Global(), argc, argv)
             .ToLocal(&result)) {
      LOG(ERROR) << "Handler function calling failed";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER);
    }

    if (result->IsPromise()) {
      std::string error_msg;
      auto execution_result =
          ExecutionUtils::V8PromiseHandler(v8_isolate, result, error_msg);
      if (!execution_result.Successful()) {
        DLOG(ERROR) << "V8 Promise execution failed" << error_msg;
        return GetError(v8_isolate, try_catch, execution_result.status_code);
      }
    }
    execution_response.metrics[kHandlerCallMetricJsEngineDuration] =
        stopwatch.GetElapsedTime();

    // Treat as JSON escaped string if there is no input_type in the metadata or
    // the metadata of input type is not for a byte string.
    if (!(uses_input_type && uses_input_type_bytes)) {
      v8::Local<v8::String> result_string;
      auto result_json_maybe = v8::JSON::Stringify(v8_context, result);
      if (!result_json_maybe.ToLocal(&result)) {
        LOG(ERROR) << "Failed to convert the V8 JSON result to Local string";
        return GetError(v8_isolate, try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_JSON);
      }
    }
    auto conversion_worked = TypeConverter<std::string>::FromV8(
        v8_isolate, result, &execution_response.response);
    if (!conversion_worked) {
      LOG(ERROR) << "Failed to convert the V8 Local string to std::string";
      return GetError(v8_isolate, try_catch,
                      SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING);
    }
  }

  return execution_response;
}

ExecutionResultOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunJs(
    std::string_view code, std::string_view function_name,
    const std::vector<absl::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    const RomaJsEngineCompilationContext& context) noexcept {
  return CompileAndRunJsWithWasm(code, absl::Span<const uint8_t>(),
                                 function_name, input, metadata, context);
}

ExecutionResultOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunWasm(
    std::string_view code, std::string_view function_name,
    const std::vector<absl::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    const RomaJsEngineCompilationContext& context) noexcept {
  JsEngineExecutionResponse execution_response;

  // temp solution for CompileAndRunWasm(). This will update in next PR soon.
  auto isolate_or = CreateIsolate();
  if (!isolate_or) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_COULD_NOT_CREATE_ISOLATE);
  }
  isolate_wrapper_ = std::move(isolate_or);

  if (!isolate_wrapper_) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED);
  }

  // Start execution watchdog to timeout the execution if it runs too long.
  StartWatchdogTimer(isolate_wrapper_->isolate(), metadata);

  std::string input_code;
  RomaJsEngineCompilationContext out_context;
  // For now we just store and reuse the actual code as context.
  if (auto context_code = GetCodeFromContext(context); context_code) {
    input_code = *context_code;
    out_context = context;
  } else {
    input_code = code;
    out_context.context = std::make_shared<std::string>(code);
  }
  execution_response.compilation_context = out_context;

  auto isolate = isolate_wrapper_->isolate();
  std::vector<std::string> errors;
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> v8_context;

  {
    auto execution_result = CreateV8Context(isolate, v8_context);
    RETURN_IF_FAILURE(execution_result);

    v8::Context::Scope context_scope(v8_context);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::TryCatch try_catch(isolate);

    std::string errors;
    if (auto result = ExecutionUtils::CompileRunWASM(input_code, errors);
        !result.Successful()) {
      LOG(ERROR) << errors;
      return GetError(isolate_wrapper_->isolate(), try_catch,
                      result.status_code);
    }

    if (!function_name.empty()) {
      v8::Local<v8::Value> wasm_handler;
      if (auto result = ExecutionUtils::GetWasmHandler(function_name,
                                                       wasm_handler, errors);
          !result.Successful()) {
        LOG(ERROR) << errors;
        return GetError(isolate_wrapper_->isolate(), try_catch,
                        result.status_code);
      }

      auto wasm_input_array = ExecutionUtils::ParseAsWasmInput(
          isolate_wrapper_->isolate(), context, input);

      if (wasm_input_array.IsEmpty() ||
          wasm_input_array->Length() != input.size()) {
        return GetError(isolate, try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_PARSE_SCRIPT_INPUT);
      }

      auto input_length = wasm_input_array->Length();
      v8::Local<v8::Value> wasm_input[input_length];
      for (size_t i = 0; i < input_length; ++i) {
        wasm_input[i] = wasm_input_array->Get(context, i).ToLocalChecked();
      }

      auto handler_function = wasm_handler.As<v8::Function>();

      v8::Local<v8::Value> wasm_result;
      if (!handler_function
               ->Call(context, context->Global(), input_length, wasm_input)
               .ToLocal(&wasm_result)) {
        return GetError(isolate_wrapper_->isolate(), try_catch,
                        SC_ROMA_V8_ENGINE_ERROR_INVOKING_HANDLER);
      }

      auto offset = wasm_result.As<v8::Int32>()->Value();
      auto wasm_execution_output = ExecutionUtils::ReadFromWasmMemory(
          isolate_wrapper_->isolate(), context, offset);
      auto result_json_maybe =
          v8::JSON::Stringify(context, wasm_execution_output);
      v8::Local<v8::String> result_json;
      if (!result_json_maybe.ToLocal(&result_json)) {
        return GetError(isolate_wrapper_->isolate(), try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING);
      }

      if (auto conversion_worked = TypeConverter<std::string>::FromV8(
              isolate, result_json,
              &execution_response.execution_response.response);
          !conversion_worked) {
        return GetError(isolate, try_catch,
                        SC_ROMA_V8_ENGINE_COULD_NOT_CONVERT_OUTPUT_TO_STRING);
      }
    }
  }

  // End execution_watchdog_ in case it terminate the standby isolate.
  StopWatchdogTimer();

  return execution_response;
}

ExecutionResultOr<JsEngineExecutionResponse>
V8JsEngine::CompileAndRunJsWithWasm(
    std::string_view code, absl::Span<const uint8_t> wasm,
    std::string_view function_name, const std::vector<absl::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    const RomaJsEngineCompilationContext& context) noexcept {
  std::string err_msg;
  JsEngineExecutionResponse execution_response;
  std::shared_ptr<SnapshotCompilationContext> curr_comp_ctx;
  if (!context) {
    auto context_or = CreateCompilationContext(code, wasm, metadata, err_msg);
    if (!context_or.result().Successful()) {
      LOG(ERROR) << std::string("CreateCompilationContext failed with ")
                 << err_msg;
      return context_or.result();
    }
    execution_response.compilation_context = context_or.value();
    curr_comp_ctx = std::static_pointer_cast<SnapshotCompilationContext>(
        context_or.value().context);
  } else {
    curr_comp_ctx =
        std::static_pointer_cast<SnapshotCompilationContext>(context.context);

    if (const auto &uuid_it = metadata.find(kRequestUuid),
        id_it = metadata.find(kRequestId);
        isolate_function_binding_ && uuid_it != metadata.end() &&
        id_it != metadata.end()) {
      isolate_function_binding_->AddIds(uuid_it->second, id_it->second);
    }
  }

  v8::Isolate* v8_isolate = curr_comp_ctx->isolate->isolate();
  if (v8_isolate == nullptr) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_ISOLATE_NOT_INITIALIZED);
  }

  // No function_name just return execution_response which may contain
  // RomaJsEngineCompilationContext.
  if (function_name.empty()) {
    return execution_response;
  }

  StartWatchdogTimer(v8_isolate, metadata);
  const auto execution_result =
      ExecuteJs(curr_comp_ctx, function_name, input, metadata);
  // End execution_watchdog_ in case it terminate the standby isolate.
  StopWatchdogTimer();
  if (execution_result.Successful()) {
    execution_response.execution_response = execution_result.value();
    return execution_response;
  }
  // Return timeout error if the watchdog called isolate terminate.
  if (execution_watchdog_->IsTerminateCalled()) {
    return FailureExecutionResult(SC_ROMA_V8_ENGINE_EXECUTION_TIMEOUT);
  }
  return execution_result.result();
}

ExecutionResult V8JsEngine::CreateV8Context(v8::Isolate* isolate,
                                            v8::Local<v8::Context>& context) {
  v8::Local<v8::ObjectTemplate> global_object_template =
      v8::ObjectTemplate::New(isolate);

  if (isolate_function_binding_) {
    auto result = isolate_function_binding_->BindFunctions(
        isolate, global_object_template);
    RETURN_IF_FAILURE(result);
  }

  context = v8::Context::New(isolate, nullptr, global_object_template);
  return SuccessExecutionResult();
}

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
