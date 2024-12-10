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
#include <string_view>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "src/roma/config/type_converter.h"
#include "src/roma/wasm/deserializer.h"
#include "src/roma/wasm/serializer.h"
#include "src/roma/wasm/wasm_types.h"
#include "src/util/status_macro/status_builder.h"
#include "src/util/status_macro/status_macros.h"

using google::scp::roma::wasm::RomaWasmListOfStringRepresentation;
using google::scp::roma::wasm::RomaWasmStringRepresentation;
using google::scp::roma::wasm::WasmDeserializer;
using google::scp::roma::wasm::WasmSerializer;

namespace google::scp::roma::worker {

namespace {

constexpr std::string_view kWasmMemory = "memory";
constexpr std::string_view kWasiSnapshotPreview = "wasi_snapshot_preview1";
constexpr std::string_view kWasiProcExitFunctionName = "proc_exit";
constexpr std::string_view kExportsTag = "exports";
constexpr std::string_view kWebAssemblyTag = "WebAssembly";
constexpr std::string_view kInstanceTag = "Instance";
constexpr std::string_view kRegisteredWasmExports = "RomaRegisteredWasmExports";

}  // namespace

absl::Status ExecutionUtils::OverrideConsoleLog(v8::Isolate* isolate,
                                                bool logging_function_set) {
  if (!logging_function_set) {
    return absl::OkStatus();
  }

  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  constexpr auto js_code = R"(
    console.log = (...inputs) => ROMA_LOG(inputs.join(' '));
    console.warn = (...inputs) => ROMA_WARN(inputs.join(' '));
    console.error = (...inputs) => ROMA_ERROR(inputs.join(' '));
  )";
  v8::Local<v8::String> source =
      v8::String::NewFromUtf8(isolate, js_code).ToLocalChecked();

  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, source).ToLocal(&script)) {
    return absl::InternalError("Failed to override console.log");
  }

  v8::Local<v8::Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    return absl::InternalError("Failed to override console.log");
  }
  return absl::OkStatus();
}

absl::Status ExecutionUtils::CompileRunJS(
    std::string_view js, bool logging_function_set,
    absl::Nullable<v8::Local<v8::UnboundScript>*> unbound_script) {
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  if (auto result = OverrideConsoleLog(isolate, logging_function_set);
      !result.ok()) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InvalidArgumentError("Failed to compile JavaScript code object"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  v8::Local<v8::String> js_source =
      v8::String::NewFromUtf8(isolate, js.data(), v8::NewStringType::kNormal,
                              js.size())
          .ToLocalChecked();
  v8::Local<v8::Script> script;
  if (!v8::Script::Compile(context, js_source).ToLocal(&script)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InvalidArgumentError("Failed to compile JavaScript code object"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  if (unbound_script) {
    *unbound_script = script->GetUnboundScript();
  }

  v8::Local<v8::Value> script_result;
  if (!script->Run(context).ToLocal(&script_result)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InvalidArgumentError("Failed to run JavaScript code object"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  return absl::OkStatus();
}

absl::Status ExecutionUtils::GetJsHandler(std::string_view handler_name,
                                          v8::Local<v8::Value>& handler) {
  if (handler_name.empty()) {
    return absl::InvalidArgumentError("Handler name cannot be empty");
  }
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  v8::Local<v8::Object> ctx = context->Global();
  for (const auto& name : absl::StrSplit(handler_name, ".")) {
    v8::Local<v8::String> local_name =
        v8::String::NewFromUtf8(isolate, name.data(),
                                v8::NewStringType::kNormal, name.size())
            .ToLocalChecked();
    // If there is no handler function, or if it is not a function,
    // bail out
    if (!ctx->Get(context, local_name).ToLocal(&handler)) {
      privacy_sandbox::server_common::StatusBuilder builder(
          absl::InvalidArgumentError("Invalid handler function"));
      builder << ExecutionUtils::DescribeError(isolate, &try_catch);
      return builder;
    }
    (void)handler->ToObject(context).ToLocal(&ctx);
  }
  if (!handler->IsFunction()) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::NotFoundError("Failed to get valid function handler"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  return absl::OkStatus();
}

absl::Status ExecutionUtils::CompileRunWASM(std::string_view wasm) {
  auto isolate = v8::Isolate::GetCurrent();
  v8::HandleScope handle_scope(isolate);
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  auto module_maybe = v8::WasmModuleObject::Compile(
      isolate,
      v8::MemorySpan<const uint8_t>(
          reinterpret_cast<const unsigned char*>(wasm.data()), wasm.length()));
  v8::Local<v8::WasmModuleObject> wasm_module;
  if (!module_maybe.ToLocal(&wasm_module)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InvalidArgumentError("Failed to compile wasm object"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  v8::Local<v8::Value> web_assembly;
  if (!context->Global()
           ->Get(context,
                 v8::String::NewFromUtf8(isolate, kWebAssemblyTag.data(),
                                         v8::NewStringType::kNormal,
                                         kWebAssemblyTag.size())
                     .ToLocalChecked())
           .ToLocal(&web_assembly)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InternalError("Failed to create wasm assembly"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  v8::Local<v8::Value> wasm_instance;
  if (!web_assembly.As<v8::Object>()
           ->Get(context, v8::String::NewFromUtf8(isolate, kInstanceTag.data(),
                                                  v8::NewStringType::kNormal,
                                                  kInstanceTag.size())
                              .ToLocalChecked())
           .ToLocal(&wasm_instance)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InternalError("Failed to create wasm instance"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  auto wasm_imports = ExecutionUtils::GenerateWasmImports(isolate);

  v8::Local<v8::Value> instance_args[] = {wasm_module, wasm_imports};
  v8::Local<v8::Value> wasm_construct;
  if (!wasm_instance.As<v8::Object>()
           ->CallAsConstructor(context, 2, instance_args)
           .ToLocal(&wasm_construct)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InternalError("Failed to create wasm construct"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  v8::Local<v8::Value> wasm_exports;
  if (!wasm_construct.As<v8::Object>()
           ->Get(context, v8::String::NewFromUtf8(isolate, kExportsTag.data(),
                                                  v8::NewStringType::kNormal,
                                                  kExportsTag.size())
                              .ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InternalError("Failed to create wasm exports"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  // Register wasm_exports object in context.
  if (!context->Global()
           ->Set(context,
                 v8::String::NewFromUtf8(isolate, kRegisteredWasmExports.data(),
                                         v8::NewStringType::kNormal,
                                         kRegisteredWasmExports.size())
                     .ToLocalChecked(),
                 wasm_exports)
           .ToChecked()) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InternalError("Failed to register wasm objects in context"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  return absl::OkStatus();
}

absl::Status ExecutionUtils::GetWasmHandler(std::string_view handler_name,
                                            v8::Local<v8::Value>& handler) {
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());

  // Get wasm export object.
  v8::Local<v8::Value> wasm_exports;
  if (!context->Global()
           ->Get(context,
                 v8::String::NewFromUtf8(isolate, kRegisteredWasmExports.data(),
                                         v8::NewStringType::kNormal,
                                         kRegisteredWasmExports.size())
                     .ToLocalChecked())
           .ToLocal(&wasm_exports)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::NotFoundError("Failed to retrieve wasm exports"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  // Fetch out the handler name from code object.
  auto str = std::string(handler_name.data(), handler_name.size());
  v8::Local<v8::String> local_name =
      TypeConverter<std::string>::ToV8(isolate, str).As<v8::String>();

  // If there is no handler function, or if it is not a function,
  // bail out
  if (!wasm_exports.As<v8::Object>()
           ->Get(context, local_name)
           .ToLocal(&handler) ||
      !handler->IsFunction()) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InvalidArgumentError("Failed to get valid function handler"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  return absl::OkStatus();
}

absl::Status ExecutionUtils::CreateUnboundScript(
    v8::Global<v8::UnboundScript>& unbound_script,
    absl::Nonnull<v8::Isolate*> isolate, std::string_view js) {
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> context = v8::Context::New(isolate);
  v8::Context::Scope context_scope(context);

  v8::Local<v8::UnboundScript> local_unbound_script;
  bool logging_function_set = false;
  PS_RETURN_IF_ERROR(ExecutionUtils::CompileRunJS(js, logging_function_set,
                                                  &local_unbound_script));

  // Store unbound_script_ in a Global handle in isolate.
  unbound_script.Reset(isolate, local_unbound_script);

  return absl::OkStatus();
}

absl::Status ExecutionUtils::BindUnboundScript(
    const v8::Global<v8::UnboundScript>& global_unbound_script) {
  auto isolate = v8::Isolate::GetCurrent();
  v8::TryCatch try_catch(isolate);
  v8::Local<v8::Context> context(isolate->GetCurrentContext());
  v8::Context::Scope context_scope(context);

  v8::Local<v8::UnboundScript> unbound_script =
      v8::Local<v8::UnboundScript>::New(isolate, global_unbound_script);

  v8::Local<v8::Value> script_result;
  if (!unbound_script->BindToCurrentContext()->Run(context).ToLocal(
          &script_result)) {
    privacy_sandbox::server_common::StatusBuilder builder(
        absl::InvalidArgumentError("Failed to bind unbound script"));
    builder << ExecutionUtils::DescribeError(isolate, &try_catch);
    return builder;
  }

  return absl::OkStatus();
}

v8::Local<v8::Value> ExecutionUtils::GetWasmMemoryObject(
    absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Context>& context) {
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
    bool is_byte_str) {
  auto isolate = v8::Isolate::GetCurrent();
  auto context = isolate->GetCurrentContext();

  if (is_wasm) {
    return ExecutionUtils::ParseAsWasmInput(isolate, context, input);
  }

  return ExecutionUtils::ParseAsJsInput(input, is_byte_str);
}

std::string ExecutionUtils::ExtractMessage(absl::Nonnull<v8::Isolate*> isolate,
                                           v8::Local<v8::Message> message) {
  std::string exception_msg;
  TypeConverter<std::string>::FromV8(isolate, message->Get(), &exception_msg);
  // We want to return a message of the form:
  //
  //     line 7: Uncaught ReferenceError: blah is not defined.
  //
  int line;
  // Sometimes for multi-line errors there is no line number.
  if (!message->GetLineNumber(isolate->GetCurrentContext()).To(&line)) {
    return exception_msg;
  }

  return absl::StrCat("line ", line, ": ", exception_msg);
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
                                v8::NewStringType::kNormal, input[i].size())
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
    absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Context>& context,
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

std::string ExecutionUtils::DescribeError(
    absl::Nonnull<v8::Isolate*> isolate,
    absl::Nonnull<v8::TryCatch*> try_catch) {
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
    absl::Nonnull<v8::Isolate*> isolate,
    v8::Local<v8::Object>& wasi_snapshot_preview_object, std::string_view name,
    v8::FunctionCallback wasi_function) {
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
static v8::Local<v8::Object> GenerateWasiObject(
    absl::Nonnull<v8::Isolate*> isolate) {
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
static void RegisterObjectInWasmImports(absl::Nonnull<v8::Isolate*> isolate,
                                        v8::Local<v8::Object>& imports_object,
                                        std::string_view name,
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
    absl::Nonnull<v8::Isolate*> isolate) {
  auto imports_object = v8::Object::New(isolate);

  auto wasi_object = GenerateWasiObject(isolate);

  RegisterObjectInWasmImports(isolate, imports_object, kWasiSnapshotPreview,
                              wasi_object);

  return imports_object;
}

v8::Local<v8::Value> ExecutionUtils::ReadFromWasmMemory(
    absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Context>& context,
    int32_t offset) {
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

absl::Status ExecutionUtils::V8PromiseHandler(
    absl::Nonnull<v8::Isolate*> isolate, v8::Local<v8::Value>& result) {
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
    privacy_sandbox::server_common::StatusBuilder builder(absl::InternalError(
        "The code object async function execution failed."));
    builder << ExecutionUtils::ExtractMessage(isolate, message);
    promise->MarkAsHandled();
    return builder;
  }

  result = promise->Result();
  return absl::OkStatus();
}

}  // namespace google::scp::roma::worker
