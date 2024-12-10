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
#include "absl/log/check.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "src/debug/debug-interface.h"
#include "src/logger/request_context_logger.h"
#include "src/roma/config/type_converter.h"
#include "src/roma/interface/function_binding_io.pb.h"
#include "src/roma/logging/logging.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/js_engine/v8_engine/profiler_isolate_wrapper.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"
#include "src/roma/worker/execution_utils.h"
#include "src/util/duration.h"
#include "src/util/process_util.h"
#include "src/util/status_macro/status_macros.h"

#include "snapshot_compilation_context.h"

using google::scp::roma::kDefaultExecutionTimeout;
using google::scp::roma::kWasmCodeArrayName;
using google::scp::roma::TypeConverter;
using google::scp::roma::logging::LogOptions;
using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::proto::RpcWrapper;
using google::scp::roma::sandbox::constants::kHandlerCallMetricJsEngineDuration;
using google::scp::roma::sandbox::constants::
    kInputParsingMetricJsEngineDuration;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupV8FlagsKey;
using google::scp::roma::sandbox::constants::kJsEngineOneTimeSetupWasmPagesKey;
using google::scp::roma::sandbox::constants::kMaxNumberOfWasm32BitMemPages;
using google::scp::roma::sandbox::constants::kMinLogLevel;
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;
using google::scp::roma::sandbox::constants::kWasmMemPagesV8PlatformFlag;
using google::scp::roma::sandbox::js_engine::JsEngineExecutionResponse;
using google::scp::roma::sandbox::js_engine::RomaJsEngineCompilationContext;
using google::scp::roma::sandbox::js_engine::v8_js_engine::
    V8IsolateFunctionBinding;
using google::scp::roma::worker::ExecutionUtils;

namespace {
absl::LogSeverity GetSeverity(std::string_view severity) {
  if (severity == "ROMA_ERROR") {
    return absl::LogSeverity::kError;
  } else if (severity == "ROMA_WARN") {
    return absl::LogSeverity::kWarning;
  } else {
    return absl::LogSeverity::kInfo;
  }
}

absl::LogSeverity GetLogLevel(std::string_view level) {
  int severity;
  if (!absl::SimpleAtoi(level, &severity)) {
    return absl::LogSeverity::kInfo;
  }
  return static_cast<absl::LogSeverity>(severity);
}

std::shared_ptr<std::string> GetCodeFromContext(
    const RomaJsEngineCompilationContext& context) {
  std::shared_ptr<std::string> code;
  if (context) {
    code = std::static_pointer_cast<std::string>(context.context);
  }
  return code;
}

std::string GetError(v8::Isolate* isolate, v8::TryCatch& try_catch,
                     std::string_view top_level_error) {
  if (std::string error_msg;
      try_catch.HasCaught() && !try_catch.Message().IsEmpty() &&
      TypeConverter<std::string>::FromV8(isolate, try_catch.Message()->Get(),
                                         &error_msg)) {
    return absl::StrCat(top_level_error, " ", error_msg);
  }
  return std::string(top_level_error);
}

std::string GetStackTrace(v8::Isolate* isolate, v8::TryCatch& try_catch,
                          v8::Local<v8::Context> context) {
  v8::MaybeLocal<v8::Value> maybe_stack_trace = try_catch.StackTrace(context);
  v8::Local<v8::Value> stack_trace_str;
  if (maybe_stack_trace.IsEmpty() ||
      !maybe_stack_trace.ToLocal(&stack_trace_str) ||
      !stack_trace_str->IsString()) {
    return "<no stack trace found>";
  }
  v8::String::Utf8Value stack_trace(isolate, stack_trace_str.As<v8::String>());
  return *stack_trace != nullptr ? std::string(*stack_trace)
                                 : "<failed to convert stack trace>";
}

RpcWrapper ConstructRpcWrapper(std::string_view function_name,
                               std::string_view log_msg, std::string_view id,
                               std::string_view uuid) {
  RpcWrapper rpc_proto;
  FunctionBindingIoProto function_proto;
  function_proto.set_input_string(log_msg);

  rpc_proto.set_function_name(function_name);
  rpc_proto.set_request_id(id);
  rpc_proto.set_request_uuid(uuid);
  *rpc_proto.mutable_io_proto() = std::move(function_proto);
  return rpc_proto;
}

LogOptions GetLogOptions(
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata) {
  absl::LogSeverity min_log_level = absl::LogSeverity::kInfo;
  if (const auto log_level_it = metadata.find(kMinLogLevel);
      log_level_it != metadata.end()) {
    min_log_level = GetLogLevel(log_level_it->second);
  }

  std::string_view uuid;
  std::string_view id;
  if (const auto uuid_it = metadata.find(kRequestUuid),
      id_it = metadata.find(kRequestId);
      uuid_it != metadata.end() && id_it != metadata.end()) {
    uuid = uuid_it->second;
    id = id_it->second;
  }

  return {
      .uuid = uuid,
      .id = id,
      .min_log_level = min_log_level,
  };
}

std::string_view GetGCTypeName(v8::GCType type) {
  static const absl::flat_hash_map<v8::GCType, std::string_view> gcTypeMap = {
      {v8::GCType::kGCTypeScavenge, "kGCTypeScavenge"},
      {v8::GCType::kGCTypeMinorMarkCompact, "kGCTypeMinorMarkCompact"},
      {v8::GCType::kGCTypeMarkSweepCompact, "kGCTypeMarkSweepCompact"},
      {v8::GCType::kGCTypeIncrementalMarking, "kGCTypeIncrementalMarking"},
      {v8::GCType::kGCTypeProcessWeakCallbacks, "kGCTypeProcessWeakCallbacks"},
      {v8::GCType::kGCTypeAll, "kGCTypeAll"},
  };

  if (auto it = gcTypeMap.find(type); it != gcTypeMap.end()) {
    return it->second;
  } else {
    return "UNKNOWN_GC_TYPE";
  }
}

std::string_view GetGCCallbackFlagsName(v8::GCCallbackFlags flags) {
  static const absl::flat_hash_map<v8::GCCallbackFlags, std::string_view>
      flagMap = {
          {v8::GCCallbackFlags::kNoGCCallbackFlags, "kNoGCCallbackFlags"},
          {v8::GCCallbackFlags::kGCCallbackFlagConstructRetainedObjectInfos,
           "kGCCallbackFlagConstructRetainedObjectInfos"},
          {v8::GCCallbackFlags::kGCCallbackFlagForced, "kGCCallbackFlagForced"},
          {v8::GCCallbackFlags::
               kGCCallbackFlagSynchronousPhantomCallbackProcessing,
           "kGCCallbackFlagSynchronousPhantomCallbackProcessing"},
          {v8::GCCallbackFlags::kGCCallbackFlagCollectAllAvailableGarbage,
           "kGCCallbackFlagCollectAllAvailableGarbage"},
          {v8::GCCallbackFlags::kGCCallbackFlagCollectAllExternalMemory,
           "kGCCallbackFlagCollectAllExternalMemory"},
          {v8::GCCallbackFlags::kGCCallbackScheduleIdleGarbageCollection,
           "kGCCallbackScheduleIdleGarbageCollection"},
      };

  if (auto it = flagMap.find(flags); it != flagMap.end()) {
    return it->second;
  } else {
    return "UNKNOWN_GC_CALLBACK_FLAG";
  }
}
}  // namespace

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

V8JsEngine::V8JsEngine(
    std::unique_ptr<V8IsolateFunctionBinding> isolate_function_binding,
    const bool skip_v8_cleanup, const bool enable_profilers,
    const JsEngineResourceConstraints& v8_resource_constraints,
    const bool logging_function_set,
    const bool disable_udf_stacktraces_in_response)
    : isolate_function_binding_(std::move(isolate_function_binding)),
      v8_resource_constraints_(v8_resource_constraints),
      execution_watchdog_(std::make_unique<roma::worker::ExecutionWatchDog>()),
      skip_v8_cleanup_(skip_v8_cleanup),
      enable_profilers_(enable_profilers),
      logging_function_set_(logging_function_set),
      disable_udf_stacktraces_in_response_(
          disable_udf_stacktraces_in_response) {
  if (isolate_function_binding_) {
    isolate_function_binding_->AddExternalReferences(external_references_);
  }
  // Must be null terminated
  external_references_.push_back(0);
}

void V8JsEngine::Run() { execution_watchdog_->Run(); }

void V8JsEngine::Stop() {
  if (execution_watchdog_) {
    execution_watchdog_->Stop();
  }
  isolate_wrapper_ = nullptr;
}

V8JsEngine::~V8JsEngine() {
  Stop();
  if (!skip_v8_cleanup_) {
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
  }
}

void V8JsEngine::OneTimeSetup(
    const absl::flat_hash_map<std::string, std::string>& config) {
  size_t max_wasm_memory_number_of_pages = 0;
  if (const auto it = config.find(kJsEngineOneTimeSetupWasmPagesKey);
      it != config.end()) {
    std::stringstream page_count_converter;
    page_count_converter << it->second;
    page_count_converter >> max_wasm_memory_number_of_pages;
  }
  absl::StatusOr<std::string> my_path =
      privacy_sandbox::server_common::GetExePath();
  CHECK_OK(my_path);
  v8::V8::InitializeICUDefaultLocation(my_path->data());
  v8::V8::InitializeExternalStartupData(my_path->data());

  std::string v8_flags;
  if (const auto it = config.find(kJsEngineOneTimeSetupV8FlagsKey);
      it != config.end() && !it->second.empty()) {
    v8_flags = it->second;
  }

  // Set the max number of WASM memory pages
  if (max_wasm_memory_number_of_pages != 0) {
    const size_t page_count = std::min(max_wasm_memory_number_of_pages,
                                       kMaxNumberOfWasm32BitMemPages);
    const auto flag_value =
        absl::StrCat(" ", kWasmMemPagesV8PlatformFlag, page_count);
    absl::StrAppend(&v8_flags, flag_value);
  }

  v8::V8::SetFlagsFromString(v8_flags.c_str());

  static const v8::Platform* v8_platform = [] {
    std::unique_ptr<v8::Platform> v8_platform =
        v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(v8_platform.get());
    v8::V8::Initialize();
    return v8_platform.release();
  }();
}

absl::Status V8JsEngine::FormatAndLogError(v8::Isolate* isolate,
                                           v8::TryCatch& try_catch,
                                           v8::Local<v8::Context> context,
                                           std::string_view top_level_error,
                                           LogOptions log_options) {
  std::string err_msg = GetError(isolate, try_catch, top_level_error);
  std::string stack_trace = GetStackTrace(isolate, try_catch, context);
  std::string err_msg_stack_trace = absl::StrCat(err_msg, "\n", stack_trace);
  HandleLog("ROMA_ERROR", err_msg_stack_trace, std::move(log_options))
      .IgnoreError();
  if (disable_udf_stacktraces_in_response_) {
    return absl::InternalError(std::move(err_msg));
  }
  return absl::InternalError(std::move(err_msg_stack_trace));
}

absl::Status V8JsEngine::HandleLog(std::string_view function_name,
                                   std::string_view msg,
                                   LogOptions log_options) {
  if (GetSeverity(function_name) < log_options.min_log_level ||
      isolate_function_binding_ == nullptr) {
    return absl::OkStatus();
  }

  auto rpc_proto =
      ConstructRpcWrapper(function_name, msg, log_options.id, log_options.uuid);
  return isolate_function_binding_->InvokeRpc(rpc_proto);
}

absl::Status V8JsEngine::CreateSnapshot(v8::StartupData& startup_data,
                                        std::string_view js_code) {
  v8::SnapshotCreator creator(external_references_.data());
  v8::Isolate* isolate = creator.GetIsolate();
  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context;
    PS_RETURN_IF_ERROR(CreateV8Context(isolate, context));

    // Create a context scope, which has essential side-effects for compilation
    v8::Context::Scope context_scope(context);
    //  Compile and run JavaScript code object.
    PS_RETURN_IF_ERROR(
        ExecutionUtils::CompileRunJS(js_code, logging_function_set_));
    // Set above context with compiled and run code as the default context for
    // the StartupData blob to create.
    creator.SetDefaultContext(context);
  }
  startup_data =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  return absl::OkStatus();
}

absl::Status V8JsEngine::CreateSnapshotWithGlobals(
    v8::StartupData& startup_data, absl::Span<const uint8_t> wasm,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata) {
  v8::SnapshotCreator creator(external_references_.data());
  v8::Isolate* isolate = creator.GetIsolate();

  {
    v8::Isolate::Scope isolate_scope(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context;
    PS_RETURN_IF_ERROR(CreateV8Context(isolate, context));

    // Create a context scope, which has essential side-effects for compilation
    v8::Context::Scope context_scope(context);
    auto wasm_code_array_name_it = metadata.find(kWasmCodeArrayName);
    if (wasm_code_array_name_it == metadata.end()) {
      LOG(ERROR) << "Wasm code array name not found in metadata: "
                 << kWasmCodeArrayName;
      return absl::InvalidArgumentError(
          "Wasm code array name not found in metadata");
    }
    std::string_view wasm_code_array_name = wasm_code_array_name_it->second;

    v8::Local<v8::String> name =
        TypeConverter<std::string>::ToV8(isolate, wasm_code_array_name)
            .As<v8::String>();
    (void)context->Global()->Set(
        context, name,
        TypeConverter<uint8_t*>::ToV8(isolate, wasm.data(), wasm.size()));
    // Set above context with compiled and run code as the default context for
    // the StartupData blob to create.
    creator.SetDefaultContext(context);
  }
  startup_data =
      creator.CreateBlob(v8::SnapshotCreator::FunctionCodeHandling::kClear);
  return absl::OkStatus();
}

static size_t NearHeapLimitCallback(void* data, size_t current_heap_limit,
                                    size_t initial_heap_limit) {
  LOG(ERROR) << "OOM in JS execution, exiting...";
  return 0;
}

void FatalErrorCallback(const char* location, const char* message) {
  LOG(ERROR) << "Fatal Error at location: "
             << (location != nullptr ? location : "unknown")
             << ". Occurred with message: "
             << (message != nullptr ? message : "");
}

void LogHeapStatistics(v8::Isolate* isolate) {
  v8::HeapStatistics heap_stats;
  isolate->GetHeapStatistics(&heap_stats);

  ROMA_VLOG(9) << "\nHeap Statistics:" << "\n  total_heap_size: "
               << heap_stats.total_heap_size()
               << "\n  total_heap_size_executable: "
               << heap_stats.total_heap_size_executable()
               << "\n  total_physical_size: "
               << heap_stats.total_physical_size()
               << "\n  used_heap_size: " << heap_stats.used_heap_size()
               << "\n  heap_size_limit: " << heap_stats.heap_size_limit();
}

void GCPrologueCallback(v8::Isolate* isolate, v8::GCType type,
                        v8::GCCallbackFlags flags) {
  ROMA_VLOG(9) << "Garbage Collection event started. Type: "
               << GetGCTypeName(type)
               << ", Flags: " << GetGCCallbackFlagsName(flags);
  LogHeapStatistics(isolate);
}

void GCEpilogueCallback(v8::Isolate* isolate, v8::GCType type,
                        v8::GCCallbackFlags flags) {
  ROMA_VLOG(9) << "Garbage Collection event finished. Type: "
               << GetGCTypeName(type)
               << ", Flags: " << GetGCCallbackFlagsName(flags);
  LogHeapStatistics(isolate);
}

std::unique_ptr<V8IsolateWrapper> V8JsEngine::CreateIsolate(
    const v8::StartupData& startup_data) {
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
  isolate->SetCaptureStackTraceForUncaughtExceptions(true);
  isolate->SetFatalErrorHandler(FatalErrorCallback);
  isolate->AddGCPrologueCallback(GCPrologueCallback);
  isolate->AddGCEpilogueCallback(GCEpilogueCallback);
  v8::debug::SetConsoleDelegate(isolate, console());
  return V8IsolateFactory::Create(isolate, std::move(allocator),
                                  enable_profilers_);
}

V8Console* V8JsEngine::console() ABSL_LOCKS_EXCLUDED(console_mutex_) {
  absl::MutexLock lock(&console_mutex_);
  if (console_ == nullptr) {
    console_ = std::make_unique<V8Console>();
  }
  return console_.get();
}

void V8JsEngine::StartWatchdogTimer(
    v8::Isolate* isolate,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata) {
  // Get the timeout value from metadata. If no timeout tag is set, the
  // default value kDefaultExecutionTimeout will be used.
  auto timeout_ms = kDefaultExecutionTimeout;
  auto timeout_str_it = metadata.find(kTimeoutDurationTag);
  if (timeout_str_it != metadata.end()) {
    std::string_view timeout_str = timeout_str_it->second;
    if (absl::Duration t; absl::ParseDuration(timeout_str, &t)) {
      timeout_ms = t;
    } else {
      LOG(ERROR) << "Timeout tag parsing with error: Could not convert timeout "
                    "tag to absl::Duration.  ";
    }
  }
  ROMA_VLOG(1) << "StartWatchdogTimer timeout set to " << timeout_ms << " ms";
  execution_watchdog_->StartTimer(isolate, timeout_ms);
}

void V8JsEngine::StopWatchdogTimer() { execution_watchdog_->EndTimer(); }

absl::StatusOr<RomaJsEngineCompilationContext>
V8JsEngine::CreateCompilationContext(
    std::string_view code, absl::Span<const uint8_t> wasm,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata) {
  if (code.empty()) {
    return absl::InvalidArgumentError(
        "Create compilation context failed with empty source code.");
  }

  auto snapshot_context = std::make_shared<SnapshotCompilationContext>();
  // If wasm code array exists, a snapshot with global wasm code array will be
  // created. Otherwise, a normal snapshot containing compiled JS code will be
  // created.
  const bool js_with_wasm = !wasm.empty();
  absl::Status snapshot_status =
      js_with_wasm ? CreateSnapshotWithGlobals(snapshot_context->startup_data,
                                               wasm, metadata)
                   : CreateSnapshot(snapshot_context->startup_data, code);
  std::unique_ptr<V8IsolateWrapper> isolate_or;
  if (snapshot_status.ok()) {
    isolate_or = CreateIsolate(snapshot_context->startup_data);
    if (!isolate_or) {
      return absl::InternalError("Creating the isolate failed.");
    }
    snapshot_context->cache_type = CacheType::kSnapshot;

    if (js_with_wasm) {
      if (auto wasm_compile_result =
              CompileWasmCodeArray(isolate_or->isolate(), wasm);
          !wasm_compile_result.ok()) {
        DLOG(ERROR) << "Compile wasm module failed with "
                    << wasm_compile_result;
        return wasm_compile_result;
      }
      if (auto status = ExecutionUtils::CreateUnboundScript(
              snapshot_context->unbound_script, isolate_or->isolate(), code);
          !status.ok()) {
        DLOG(ERROR) << "CreateUnboundScript failed with " << status.message();
        return status;
      }
      snapshot_context->cache_type = CacheType::kUnboundScript;
    }

    ROMA_VLOG(2) << "compilation context cache type is V8 snapshot";
  } else {
    DLOG(ERROR) << "CreateSnapshot failed with debug errors "
                << snapshot_status;
    // Return the failure if it isn't caused by global WebAssembly.
    if (!ExecutionUtils::CheckErrorWithWebAssembly(snapshot_status.message())) {
      return snapshot_status;
    }

    isolate_or = CreateIsolate();
    if (!isolate_or) {
      return absl::InternalError("Creating the isolate failed.");
    }

    if (auto status = ExecutionUtils::CreateUnboundScript(
            snapshot_context->unbound_script, isolate_or->isolate(), code);
        !status.ok()) {
      DLOG(ERROR) << "CreateUnboundScript failed with " << status.message();
      return status;
    }

    snapshot_context->cache_type = CacheType::kUnboundScript;
    ROMA_VLOG(2) << "compilation context cache type is V8 UnboundScript";
  }

  // Snapshot the isolate with compilation context and also initialize a
  // execution watchdog inside the isolate.
  snapshot_context->isolate = std::move(isolate_or);
  return RomaJsEngineCompilationContext{
      .context = snapshot_context,
  };
}

absl::Status V8JsEngine::CompileWasmCodeArray(v8::Isolate* isolate,
                                              absl::Span<const uint8_t> wasm) {
  v8::Isolate::Scope isolate_scope(isolate);
  // Create a handle scope to keep the temporary object references.
  v8::HandleScope handle_scope(isolate);
  // Set up an exception handler before calling the Process function
  v8::TryCatch try_catch(isolate);

  // Create a context scope, which has essential side-effects for compilation
  v8::Local<v8::Context> v8_context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(v8_context);

  // Check whether wasm module can compile
  if (const auto module_maybe = v8::WasmModuleObject::Compile(
          isolate, v8::MemorySpan<const uint8_t>(
                       reinterpret_cast<const unsigned char*>(wasm.data()),
                       wasm.size()));
      module_maybe.IsEmpty()) {
    return absl::InternalError("Failed to compile wasm object.");
  }
  return absl::OkStatus();
}

absl::StatusOr<ExecutionResponse> V8JsEngine::ExecuteJs(
    const std::shared_ptr<SnapshotCompilationContext>&
        current_compilation_context,
    std::string_view function_name, const std::vector<std::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata) {
  LogOptions log_options = GetLogOptions(metadata);

  v8::Isolate* v8_isolate = current_compilation_context->isolate->isolate();
  v8::Isolate::Scope isolate_scope(v8_isolate);
  // Create a handle scope to keep the temporary object references.
  v8::HandleScope handle_scope(v8_isolate);
  // Set up an exception handler before calling the Process function
  v8::TryCatch try_catch(v8_isolate);

  // Create a context scope, which has essential side-effects for compilation
  v8::Local<v8::Context> v8_context = v8::Context::New(v8_isolate);
  v8::Context::Scope context_scope(v8_context);

  // Binding UnboundScript to current context when the compilation context is
  // kUnboundScript.
  if (current_compilation_context->cache_type == CacheType::kUnboundScript) {
    if (auto status = ExecutionUtils::BindUnboundScript(
            current_compilation_context->unbound_script);
        !status.ok()) {
      LOG(ERROR)
          << "BindUnboundScript failed with: Failed to bind unbound script.";
      DLOG(ERROR) << "BindUnboundScript failed with debug errors "
                  << status.message();
      return status;
    }
  }

  v8::Local<v8::Value> handler;
  if (const auto status = ExecutionUtils::GetJsHandler(function_name, handler);
      !status.ok()) {
    DLOG(ERROR) << "GetJsHandler failed with " << status.message();
    return status;
  }

  ExecutionResponse execution_response;
  privacy_sandbox::server_common::Stopwatch stopwatch;
  {
    v8::Local<v8::Function> handler_func = handler.As<v8::Function>();
    stopwatch.Reset();

    const auto input_type =
        metadata.find(google::scp::roma::sandbox::constants::kInputType);
    const bool uses_input_type = (input_type != metadata.end());
    const bool uses_input_type_bytes =
        (uses_input_type &&
         input_type->second ==
             google::scp::roma::sandbox::constants::kInputTypeBytes);

    v8::Local<v8::Array> argv_array =
        ExecutionUtils::ParseAsJsInput(input, uses_input_type_bytes);
    const size_t argc = input.size();
    // If argv_array size doesn't match with input. Input conversion failed.
    if (argv_array.IsEmpty() || argv_array->Length() != argc) {
      LOG(ERROR) << "Could not parse the inputs";
      return FormatAndLogError(v8_isolate, try_catch, v8_context,
                               "Error parsing input as valid JSON.",
                               std::move(log_options));
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
      return FormatAndLogError(v8_isolate, try_catch, v8_context,
                               "Error when invoking the handler.",
                               std::move(log_options));
    }
    if (result->IsPromise()) {
      std::string error_msg;
      if (auto status = ExecutionUtils::V8PromiseHandler(v8_isolate, result);
          !status.ok()) {
        DLOG(ERROR) << "V8 Promise execution failed" << status;
        return FormatAndLogError(v8_isolate, try_catch, v8_context,
                                 status.message(), std::move(log_options));
      }
    }
    execution_response.metrics[kHandlerCallMetricJsEngineDuration] =
        stopwatch.GetElapsedTime();
    // Treat as JSON escaped string if there is no input_type in the metadata or
    // the metadata of input type is not for a byte string.
    if (!(uses_input_type && uses_input_type_bytes)) {
      v8::Local<v8::String> result_string;
      if (auto result_json_maybe = v8::JSON::Stringify(v8_context, result);
          !result_json_maybe.ToLocal(&result)) {
        LOG(ERROR) << "Failed to convert the V8 JSON result to Local string";
        return FormatAndLogError(v8_isolate, try_catch, v8_context,
                                 "Error converting output to JSON.",
                                 std::move(log_options));
      }
    }
    if (!TypeConverter<std::string>::FromV8(v8_isolate, result,
                                            &execution_response.response)) {
      LOG(ERROR) << "Failed to convert V8 string to std::string";
      return FormatAndLogError(v8_isolate, try_catch, v8_context,
                               "Error converting V8 string to std::string",
                               std::move(log_options));
    }
  }
  return execution_response;
}

absl::StatusOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunJs(
    std::string_view code, std::string_view function_name,
    const std::vector<std::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    const RomaJsEngineCompilationContext& context) {
  return CompileAndRunJsWithWasm(code, absl::Span<const uint8_t>(),
                                 function_name, input, metadata, context);
}

absl::StatusOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunWasm(
    std::string_view code, std::string_view function_name,
    const std::vector<std::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    const RomaJsEngineCompilationContext& context) {
  LogOptions log_options = GetLogOptions(metadata);
  JsEngineExecutionResponse execution_response;

  if (auto isolate_or = CreateIsolate(); isolate_or) {
    isolate_wrapper_ = std::move(isolate_or);
  } else {
    return absl::InternalError("Creating the isolate failed.");
  }

  if (!isolate_wrapper_) {
    return absl::InternalError(
        "The v8 isolate has not been initialized. The module has not "
        "been initialized.");
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
    PS_RETURN_IF_ERROR(CreateV8Context(isolate, v8_context));

    // Create a context scope, which has essential side-effects for compilation
    v8::Context::Scope context_scope(v8_context);
    v8::Local<v8::Context> context(isolate->GetCurrentContext());
    v8::TryCatch try_catch(isolate);

    if (const auto status = ExecutionUtils::CompileRunWASM(input_code);
        !status.ok()) {
      DLOG(ERROR) << status.message();
      return status;
    }

    if (!function_name.empty()) {
      v8::Local<v8::Value> wasm_handler;
      if (auto status =
              ExecutionUtils::GetWasmHandler(function_name, wasm_handler);
          !status.ok()) {
        DLOG(ERROR) << status.message();
        return status;
      }

      const auto wasm_input_array = ExecutionUtils::ParseAsWasmInput(
          isolate_wrapper_->isolate(), context, input);

      if (wasm_input_array.IsEmpty() ||
          wasm_input_array->Length() != input.size()) {
        return FormatAndLogError(isolate, try_catch, context,
                                 "Error parsing input as valid JSON.",
                                 std::move(log_options));
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
        return FormatAndLogError(isolate_wrapper_->isolate(), try_catch,
                                 context, "Error when invoking the handler.",
                                 std::move(log_options));
      }

      const auto offset = wasm_result.As<v8::Int32>()->Value();
      const auto wasm_execution_output = ExecutionUtils::ReadFromWasmMemory(
          isolate_wrapper_->isolate(), context, offset);
      const auto result_json_maybe =
          v8::JSON::Stringify(context, wasm_execution_output);
      v8::Local<v8::String> result_json;
      if (!result_json_maybe.ToLocal(&result_json)) {
        return FormatAndLogError(isolate_wrapper_->isolate(), try_catch,
                                 context,
                                 "Error converting output to native string.",
                                 std::move(log_options));
      }

      if (!TypeConverter<std::string>::FromV8(
              isolate, result_json,
              &execution_response.execution_response.response)) {
        return FormatAndLogError(isolate, try_catch, context,
                                 "Error converting output to native string.",
                                 std::move(log_options));
      }
    }
  }
  // End execution_watchdog_ in case it terminate the standby isolate.
  StopWatchdogTimer();
  return execution_response;
}

absl::StatusOr<JsEngineExecutionResponse> V8JsEngine::CompileAndRunJsWithWasm(
    std::string_view code, absl::Span<const uint8_t> wasm,
    std::string_view function_name, const std::vector<std::string_view>& input,
    const absl::flat_hash_map<std::string_view, std::string_view>& metadata,
    const RomaJsEngineCompilationContext& context)
    ABSL_LOCKS_EXCLUDED(console_mutex_) {
  JsEngineExecutionResponse execution_response;
  std::shared_ptr<SnapshotCompilationContext> curr_comp_ctx;
  if (!context) {
    PS_ASSIGN_OR_RETURN(auto comp_context,
                        CreateCompilationContext(code, wasm, metadata));
    execution_response.compilation_context = comp_context;
    curr_comp_ctx = std::static_pointer_cast<SnapshotCompilationContext>(
        comp_context.context);

  } else {
    curr_comp_ctx =
        std::static_pointer_cast<SnapshotCompilationContext>(context.context);
    auto [uuid, id, min_log_level] = GetLogOptions(metadata);

    if (isolate_function_binding_) {
      isolate_function_binding_->SetMinLogLevel(min_log_level);
      isolate_function_binding_->AddIds(uuid, id);
    }
  }
  v8::Isolate* v8_isolate = curr_comp_ctx->isolate->isolate();
  if (v8_isolate == nullptr) {
    return absl::FailedPreconditionError(
        "The v8 isolate has not been initialized. The module has not "
        "been initialized.");
  }
  // No function_name just return execution_response which may contain
  // RomaJsEngineCompilationContext.
  if (function_name.empty()) {
    return execution_response;
  }
  StartWatchdogTimer(v8_isolate, metadata);
  const auto status_or_response =
      ExecuteJs(curr_comp_ctx, function_name, input, metadata);
  // End execution_watchdog_ in case it terminate the standby isolate.
  StopWatchdogTimer();
  if (status_or_response.ok()) {
    execution_response.execution_response = status_or_response.value();
    if (enable_profilers_) {
      execution_response.execution_response.profiler_output =
          static_cast<ProfilerIsolateWrapperImpl*>(curr_comp_ctx->isolate.get())
              ->StopProfiling();
    }
    return execution_response;
  }
  // Return timeout error if the watchdog called isolate terminate.
  if (execution_watchdog_->IsTerminateCalled()) {
    return absl::ResourceExhaustedError(
        "V8 execution terminated due to timeout.");
  }
  return status_or_response.status();
}

absl::Status V8JsEngine::CreateV8Context(v8::Isolate* isolate,
                                         v8::Local<v8::Context>& context) {
  v8::Local<v8::ObjectTemplate> global_object_template =
      v8::ObjectTemplate::New(isolate);
  if (isolate_function_binding_) {
    if (!isolate_function_binding_->BindFunctions(isolate,
                                                  global_object_template)) {
      return absl::InvalidArgumentError(
          "The v8 isolate passed to the visitor is invalid.");
    }
  }
  context = v8::Context::New(isolate, nullptr, global_object_template);
  return absl::OkStatus();
}

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
