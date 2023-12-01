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

#include "execution_utils.h"

#include <algorithm>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "absl/strings/str_format.h"
#include "public/core/interface/execution_result.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::StatusCode;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME;
using google::scp::core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE;
using google::scp::core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE;
using google::scp::roma::wasm::RomaWasmListOfStringRepresentation;
using google::scp::roma::wasm::RomaWasmStringRepresentation;
using google::scp::roma::wasm::WasmDeserializer;
using google::scp::roma::wasm::WasmSerializer;

namespace google::scp::roma::worker {
static constexpr char kWasmMemory[] = "memory";
static constexpr char kWasiSnapshotPreview[] = "wasi_snapshot_preview1";
static constexpr char kWasiProcExitFunctionName[] = "proc_exit";

ExecutionResult ExecutionUtils::CreatePerformanceNow(v8::Isolate* isolate) {
  v8::Local<v8::Context> context(isolate->GetCurrentContext());
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate,
                              "const performance = { now: () => Date.now() };")
          .ToLocalChecked();

  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, source).ToLocal(&script)) {
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE);
  }

  v8::Local<v8::Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::CreateNativeLogFunctions(v8::Isolate* isolate) {
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  constexpr auto js_code = R"(
    if (typeof(roma) === 'undefined') {
      var roma = {};
    }

    if (typeof(ROMA_LOG) !== 'undefined') {
      roma.n_log = ROMA_LOG;
      roma.n_warn = ROMA_WARN;
      roma.n_error = ROMA_ERROR;
    }
  )";
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, js_code).ToLocalChecked();

  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, source).ToLocal(&script)) {
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE);
  }

  v8::Local<v8::Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::CompileRunJS(
    const std::string& js, std::string& err_msg,
    v8::Local<v8::UnboundScript>* unbound_script) noexcept {
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  if (auto result = CreatePerformanceNow(isolate); !result.Successful()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return result;
  }

  if (auto result = CreateNativeLogFunctions(isolate); !result.Successful()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return result;
  }

  v8::Local<v8::String> js_source =
      v8::String::NewFromUtf8(isolate, js.data(), v8::NewStringType::kNormal,
                              static_cast<uint32_t>(js.length()))
          .ToLocalChecked();
  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, js_source).ToLocal(&script)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_CODE_COMPILE_FAILURE);
  }

  if (unbound_script) {
    *unbound_script = script->GetUnboundScript();
  }

  v8::Local<v8::Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_SCRIPT_RUN_FAILURE);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetJsHandler(const std::string& handler_name,
                                             v8::Local<v8::Value>& handler,
                                             std::string& err_msg) noexcept {
  if (handler_name.empty()) {
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_BAD_HANDLER_NAME);
  }
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  v8::Local<v8::String> local_name =
      v8::String::NewFromUtf8(isolate, handler_name.data(),
                              v8::NewStringType::kNormal,
                              static_cast<uint32_t>(handler_name.length()))
          .ToLocalChecked();

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!context->Global()->Get(context, local_name).ToLocal(&handler) ||
      !handler->IsFunction()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::CompileRunWASM(const std::string& wasm,
                                               std::string& err_msg) noexcept {
  auto isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  auto module_maybe = v8::WasmModuleObject::Compile(
      isolate,
      v8::MemorySpan<const uint8_t>(
          reinterpret_cast<const unsigned char*>(wasm.c_str()), wasm.length()));
  v8::Local<v8::WasmModuleObject> wasm_module;
  if (!module_maybe.ToLocal(&wasm_module)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_COMPILE_FAILURE);
  }

  v8::Local<v8::Value> web_assembly;
  if (!context->Global()
           ->Get(context, v8::String::NewFromUtf8(isolate, kWebAssemblyTag)
                              .ToLocalChecked())
           .ToLocal(&web_assembly)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  v8::Local<v8::Value> wasm_instance;
  if (!web_assembly.As<v8::Object>()
           ->Get(
               context,
               v8::String::NewFromUtf8(isolate, kInstanceTag).ToLocalChecked())
           .ToLocal(&wasm_instance)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  auto wasm_imports = ExecutionUtils::GenerateWasmImports(isolate);

  v8::Local<v8::Value> instance_args[] = {wasm_module, wasm_imports};
  v8::Local<v8::Value> wasm_construct;
  if (!wasm_instance.As<v8::Object>()
           ->CallAsConstructor(context, 2, instance_args)
           .ToLocal(&wasm_construct)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  v8::Local<v8::Value> wasm_exports;
  if (!wasm_construct.As<v8::Object>()
           ->Get(context,
                 v8::String::NewFromUtf8(isolate, kExportsTag).ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  // Register wasm_exports object in context.
  if (!context->Global()
           ->Set(context,
                 v8::String::NewFromUtf8(isolate, kRegisteredWasmExports)
                     .ToLocalChecked(),
                 wasm_exports)
           .ToChecked()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_CREATION_FAILURE);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::GetWasmHandler(const std::string& handler_name,
                                               v8::Local<v8::Value>& handler,
                                               std::string& err_msg) noexcept {
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  // Get wasm export object.
  v8::Local<v8::Value> wasm_exports;
  if (!context->Global()
           ->Get(context,
                 v8::String::NewFromUtf8(isolate, kRegisteredWasmExports)
                     .ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_WASM_OBJECT_RETRIEVAL_FAILURE);
  }

  // Fetch out the handler name from code object.
  auto str = std::string(handler_name.c_str(), handler_name.size());
  v8::Local<v8::String> local_name =
      TypeConverter<std::string>::ToV8(isolate, str).As<v8::String>();

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!wasm_exports.As<v8::Object>()
           ->Get(context, local_name)
           .ToLocal(&handler) ||
      !handler->IsFunction()) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_HANDLER_INVALID_FUNCTION);
  }

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::CreateUnboundScript(
    v8::Global<v8::UnboundScript>& unbound_script, v8::Isolate* isolate,
    const std::string& js, std::string& err_msg) noexcept {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::UnboundScript> local_unbound_script;
  auto execution_result =
      ExecutionUtils::CompileRunJS(js, err_msg, &local_unbound_script);
  RETURN_IF_FAILURE(execution_result);

  // Store unbound_script_ in a Global handle in isolate.
  unbound_script.Reset(isolate, local_unbound_script);

  return core::SuccessExecutionResult();
}

ExecutionResult ExecutionUtils::BindUnboundScript(
    const v8::Global<v8::UnboundScript>& global_unbound_script,
    std::string& err_msg) noexcept {
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::UnboundScript> unbound_script =
      v8::Local<v8::UnboundScript>::New(isolate, global_unbound_script);

  v8::Local<v8::Value> script_result;
  if (!unbound_script->BindToCurrentContext()->Run(context).ToLocal(
          &script_result)) {
    err_msg = ExecutionUtils::DescribeError(isolate, &try_catch);
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_BIND_UNBOUND_SCRIPT_FAILED);
  }

  return core::SuccessExecutionResult();
}

v8::Local<v8::Value> ExecutionUtils::GetWasmMemoryObject(
    v8::Isolate* isolate, v8::Local<v8::Context>& context) {
  auto wasm_exports = context->Global()
                          ->Get(context, TypeConverter<std::string>::ToV8(
                                             isolate, kRegisteredWasmExports))
                          .ToLocalChecked()
                          .As<v8::Object>();

  auto wasm_memory_maybe = wasm_exports->Get(
      context, TypeConverter<std::string>::ToV8(isolate, kWasmMemory));

  if (wasm_memory_maybe.IsEmpty()) {
    return v8::Undefined(isolate);
  }

  return wasm_memory_maybe.ToLocalChecked();
}

v8::Local<v8::Array> ExecutionUtils::InputToLocalArgv(
    const std::vector<std::string_view>& input, bool is_wasm,
    bool is_byte_str) noexcept {
  auto isolate = v8::Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  if (is_wasm) {
    return ExecutionUtils::ParseAsWasmInput(isolate, context, input);
  }

  return ExecutionUtils::ParseAsJsInput(input, is_byte_str);
}

std::string ExecutionUtils::ExtractMessage(
    v8::Isolate* isolate, v8::Local<v8::Message> message) noexcept {
  std::string exception_msg;
  TypeConverter<std::string>::FromV8(isolate, message->Get(), &exception_msg);
  // We want to return a message of the form:
  //
  //     line 7: Uncaught ReferenceError: blah is not defined.
  //
  int line;
  // Sometimes for multi-line errors there is no line number.
  if (!message->GetLineNumber(isolate->GetCurrentContext()).To(&line)) {
    return absl::StrFormat("%s", exception_msg);
  }

  return absl::StrFormat("line %i: %s", line, exception_msg);
}

v8::Local<v8::Array> ExecutionUtils::ParseAsJsInput(
    const std::vector<std::string_view>& input, bool is_byte_str) {
  auto isolate = v8::Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  const int argc = input.size();

  v8::Local<v8::Array> argv = v8::Array::New(isolate, argc);
  for (auto i = 0; i < argc; ++i) {
    v8::Local<v8::String> arg_str =
        v8::String::NewFromUtf8(isolate, input[i].data(),
                                v8::NewStringType::kNormal,
                                static_cast<uint32_t>(input[i].length()))
            .ToLocalChecked();

    if (is_byte_str) {
      if (!argv->Set(context, i, arg_str).ToChecked()) {
        return v8::Local<v8::Array>();
      }
    } else {
      v8::Local<v8::Value> arg = v8::Undefined(isolate);
      if (arg_str->Length() > 0 &&
          !v8::JSON::Parse(context, arg_str).ToLocal(&arg)) {
        return v8::Local<v8::Array>();
      }
      if (!argv->Set(context, i, arg).ToChecked()) {
        return v8::Local<v8::Array>();
      }
    }
  }

  return argv;
}

v8::Local<v8::Array> ExecutionUtils::ParseAsWasmInput(
    v8::Isolate* isolate, v8::Local<v8::Context>& context,
    const std::vector<std::string_view>& input) {
  // Parse it into JS types so we can distinguish types
  auto parsed_args = ExecutionUtils::ParseAsJsInput(input);
  const int argc = parsed_args.IsEmpty() ? 0 : parsed_args->Length();

  // Parsing the input failed
  if (argc != input.size()) {
    return v8::Local<v8::Array>();
  }

  v8::Local<v8::Array> argv = v8::Array::New(isolate, argc);

  auto wasm_memory = ExecutionUtils::GetWasmMemoryObject(isolate, context);

  if (wasm_memory->IsUndefined()) {
    // The module has no memory object. This is either a very basic WASM, or
    // invalid, we'll just exit early, and pass the input as it was parsed.
    return parsed_args;
  }

  auto wasm_memory_blob = wasm_memory.As<v8::WasmMemoryObject>()
                              ->Buffer()
                              ->GetBackingStore()
                              ->Data();
  auto wasm_memory_size = wasm_memory.As<v8::WasmMemoryObject>()
                              ->Buffer()
                              ->GetBackingStore()
                              ->ByteLength();

  size_t wasm_memory_offset = 0;

  for (auto i = 0; i < argc; ++i) {
    auto arg = parsed_args->Get(context, i).ToLocalChecked();

    // We only support uint/int, string and array of string args
    if (!arg->IsUint32() && !arg->IsInt32() && !arg->IsString() &&
        !arg->IsArray()) {
      argv.Clear();
      return v8::Local<v8::Array>();
    }

    v8::Local<v8::Value> new_arg;

    if (arg->IsUint32() || arg->IsInt32()) {
      // No serialization needed
      new_arg = arg;
    }
    if (arg->IsString()) {
      std::string str_value;
      TypeConverter<std::string>::FromV8(isolate, arg, &str_value);
      auto string_ptr_in_wasm_memory = wasm::WasmSerializer::WriteCustomString(
          wasm_memory_blob, wasm_memory_size, wasm_memory_offset, str_value);

      // The serialization failed
      if (string_ptr_in_wasm_memory == UINT32_MAX) {
        return v8::Local<v8::Array>();
      }

      new_arg =
          TypeConverter<uint32_t>::ToV8(isolate, string_ptr_in_wasm_memory);
      wasm_memory_offset +=
          wasm::RomaWasmStringRepresentation::ComputeMemorySizeFor(str_value);
    }
    if (arg->IsArray()) {
      std::vector<std::string> vec_value;
      bool worked = TypeConverter<std::vector<std::string>>::FromV8(
          isolate, arg, &vec_value);

      if (!worked) {
        // This means the array is not an array of string
        return v8::Local<v8::Array>();
      }

      auto list_ptr_in_wasm_memory =
          wasm::WasmSerializer::WriteCustomListOfString(
              wasm_memory_blob, wasm_memory_size, wasm_memory_offset,
              vec_value);

      // The serialization failed
      if (list_ptr_in_wasm_memory == UINT32_MAX) {
        return v8::Local<v8::Array>();
      }

      new_arg = TypeConverter<uint32_t>::ToV8(isolate, list_ptr_in_wasm_memory);
      wasm_memory_offset +=
          wasm::RomaWasmListOfStringRepresentation::ComputeMemorySizeFor(
              vec_value);
    }

    if (!argv->Set(context, i, new_arg).ToChecked()) {
      return v8::Local<v8::Array>();
    }
  }

  return argv;
}

std::string ExecutionUtils::DescribeError(v8::Isolate* isolate,
                                          v8::TryCatch* try_catch) noexcept {
  const v8::Local<v8::Message> message = try_catch->Message();
  if (message.IsEmpty()) {
    return std::string();
  }

  return ExecutionUtils::ExtractMessage(isolate, message);
}

/**
 * @brief Handler for the WASI proc_exit function
 *
 * @param info
 */
static void WasiProcExit(const v8::FunctionCallbackInfo<v8::Value>& info) {
  (void)info;
  auto isolate = info.GetIsolate();
  isolate->TerminateExecution();
}

/**
 * @brief Register a function in the object that represents the
 * wasi_snapshot_preview1 module
 *
 * @param isolate
 * @param wasi_snapshot_preview_object
 * @param name
 * @param wasi_function
 */
static void RegisterWasiFunction(
    v8::Isolate* isolate, v8::Local<v8::Object>& wasi_snapshot_preview_object,
    const std::string& name, v8::FunctionCallback wasi_function) {
  auto context = isolate->GetCurrentContext();

  auto func_name = TypeConverter<std::string>::ToV8(isolate, name);
  wasi_snapshot_preview_object
      ->Set(context, func_name,
            v8::FunctionTemplate::New(isolate, wasi_function)
                ->GetFunction(context)
                .ToLocalChecked()
                .As<v8::Object>())
      .Check();
}

/**
 * @brief Generate an object which represents the wasi_snapshot_preview1 module
 *
 * @param isolate
 * @return v8::Local<v8::Object>
 */
static v8::Local<v8::Object> GenerateWasiObject(v8::Isolate* isolate) {
  // Register WASI runtime allowed functions
  auto wasi_snapshot_preview_object = v8::Object::New(isolate);

  RegisterWasiFunction(isolate, wasi_snapshot_preview_object,
                       kWasiProcExitFunctionName, &WasiProcExit);

  return wasi_snapshot_preview_object;
}

/**
 * @brief Register an object in the WASM imports module
 *
 * @param isolate
 * @param imports_object
 * @param name
 * @param new_object
 */
static void RegisterObjectInWasmImports(v8::Isolate* isolate,
                                        v8::Local<v8::Object>& imports_object,
                                        const std::string& name,
                                        v8::Local<v8::Object>& new_object) {
  auto context = isolate->GetCurrentContext();

  auto obj_name = TypeConverter<std::string>::ToV8(isolate, name);
  imports_object->Set(context, obj_name, new_object).Check();
}

/**
 * @brief Generate an object that represents the WASM imports modules
 *
 * @param isolate
 * @return v8::Local<v8::Object>
 */
v8::Local<v8::Object> ExecutionUtils::GenerateWasmImports(
    v8::Isolate* isolate) {
  auto imports_object = v8::Object::New(isolate);

  auto wasi_object = GenerateWasiObject(isolate);

  RegisterObjectInWasmImports(isolate, imports_object, kWasiSnapshotPreview,
                              wasi_object);

  return imports_object;
}

v8::Local<v8::Value> ExecutionUtils::ReadFromWasmMemory(
    v8::Isolate* isolate, v8::Local<v8::Context>& context, int32_t offset) {
  if (offset < 0) {
    return v8::Undefined(isolate);
  }

  auto wasm_memory_maybe = GetWasmMemoryObject(isolate, context);
  if (wasm_memory_maybe->IsUndefined()) {
    return v8::Undefined(isolate);
  }

  auto wasm_memory = wasm_memory_maybe.As<v8::WasmMemoryObject>()
                         ->Buffer()
                         ->GetBackingStore()
                         ->Data();
  auto wasm_memory_size = wasm_memory_maybe.As<v8::WasmMemoryObject>()
                              ->Buffer()
                              ->GetBackingStore()
                              ->ByteLength();
  auto wasm_memory_blob = static_cast<uint8_t*>(wasm_memory);

  v8::Local<v8::Value> ret_val = v8::Undefined(isolate);

  std::string read_str;
  WasmDeserializer::ReadCustomString(wasm_memory_blob, wasm_memory_size, offset,
                                     read_str);
  if (read_str.empty()) {
    return v8::Undefined(isolate);
  }

  ret_val = TypeConverter<std::string>::ToV8(isolate, read_str);

  return ret_val;
}

ExecutionResult ExecutionUtils::V8PromiseHandler(v8::Isolate* isolate,
                                                 v8::Local<v8::Value>& result,
                                                 std::string& err_msg) {
  // We don't need a callback handler for now. The default handler will wrap
  // the successful result of Promise::kFulfilled and the exception message of
  // Promise::kRejected.
  auto promise = result.As<v8::Promise>();

  // Wait until promise state isn't pending.
  while (promise->State() == v8::Promise::kPending) {
    isolate->PerformMicrotaskCheckpoint();
  }

  if (promise->State() == v8::Promise::kRejected) {
    // Extract the exception message from a rejected promise.
    const v8::Local<v8::Message> message =
        v8::Exception::CreateMessage(isolate, promise->Result());
    err_msg = ExecutionUtils::ExtractMessage(isolate, message);
    promise->MarkAsHandled();
    return core::FailureExecutionResult(
        core::errors::SC_ROMA_V8_WORKER_ASYNC_EXECUTION_FAILED);
  }

  result = promise->Result();
  return core::SuccessExecutionResult();
}

}  // namespace google::scp::roma::worker
