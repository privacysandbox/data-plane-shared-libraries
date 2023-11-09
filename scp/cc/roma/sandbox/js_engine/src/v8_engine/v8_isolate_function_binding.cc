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

#include "v8_isolate_function_binding.h"

#include <memory>
#include <string>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "roma/config/src/type_converter.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "scp/cc/roma/interface/function_binding_io.pb.h"

#include "error_codes.h"

using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::
    SC_ROMA_V8_ENGINE_COULD_NOT_REGISTER_FUNCTION_BINDING;
using google::scp::core::errors::
    SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_EMPTY_CONTEXT;
using google::scp::core::errors::
    SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_INVALID_ISOLATE;
using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::sandbox::constants::kMetadataRomaRequestId;

static constexpr char kCouldNotRunFunctionBinding[] =
    "ROMA: Could not run C++ function binding.";
static constexpr char kUnexpectedDataInBindingCallback[] =
    "ROMA: Unexpected data in global callback.";
static constexpr char kCouldNotConvertJsFunctionInputToNative[] =
    "ROMA: Could not convert JS function input to native C++ type.";
static constexpr char kErrorInFunctionBindingInvocation[] =
    "ROMA: Error while executing native function binding.";

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
namespace {

bool V8TypesToProto(const v8::FunctionCallbackInfo<v8::Value>& info,
                    FunctionBindingIoProto& proto) {
  if (info.Length() == 0) {
    // No arguments were passed to function
    return true;
  }
  if (info.Length() > 1) {
    return false;
  }

  auto isolate = info.GetIsolate();
  const auto& function_parameter = info[0];

  using StrVec = std::vector<std::string>;
  using StrMap = absl::flat_hash_map<std::string, std::string>;

  // Try to convert to one of the supported types
  if (std::string string_native; TypeConverter<std::string>::FromV8(
          isolate, function_parameter, &string_native)) {
    proto.set_input_string(std::move(string_native));
  } else if (StrVec vector_of_string_native; TypeConverter<StrVec>::FromV8(
                 isolate, function_parameter, &vector_of_string_native)) {
    proto.mutable_input_list_of_string()->mutable_data()->Reserve(
        vector_of_string_native.size());
    for (auto&& native : vector_of_string_native) {
      proto.mutable_input_list_of_string()->mutable_data()->Add(
          std::move(native));
    }
  } else if (StrMap map_of_string_native; TypeConverter<StrMap>::FromV8(
                 isolate, function_parameter, &map_of_string_native)) {
    for (auto&& [key, value] : map_of_string_native) {
      (*proto.mutable_input_map_of_string()->mutable_data())[std::move(key)] =
          std::move(value);
    }
  } else if (function_parameter->IsUint8Array()) {
    const auto array = function_parameter.As<v8::Uint8Array>();
    const auto data_len = array->Length();
    std::string native_data;
    native_data.resize(data_len);
    if (!TypeConverter<uint8_t*>::FromV8(isolate, function_parameter,
                                         native_data)) {
      return false;
    }
    *proto.mutable_input_bytes() = std::move(native_data);
  } else {
    // Unknown type
    return false;
  }

  return true;
}

v8::Local<v8::Value> ProtoToV8Type(v8::Isolate* isolate,
                                   const FunctionBindingIoProto& proto) {
  if (proto.has_output_string()) {
    return TypeConverter<std::string>::ToV8(isolate, proto.output_string());
  } else if (proto.has_output_list_of_string()) {
    return TypeConverter<std::vector<std::string>>::ToV8(
        isolate, proto.output_list_of_string().data());
  } else if (proto.has_output_map_of_string()) {
    return TypeConverter<absl::flat_hash_map<std::string, std::string>>::ToV8(
        isolate, proto.output_map_of_string().data());
  } else if (proto.has_output_bytes()) {
    return TypeConverter<uint8_t*>::ToV8(isolate, proto.output_bytes());
  }

  // This function didn't return anything from C++
  ROMA_VLOG(1) << "Function binding did not set any return value from C++.";
  return v8::Undefined(isolate);
}

}  // namespace

void V8IsolateFunctionBinding::GlobalV8FunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  auto isolate = info.GetIsolate();
  auto context = isolate->GetCurrentContext();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  auto data = info.Data();
  if (data.IsEmpty()) {
    isolate->ThrowError(kUnexpectedDataInBindingCallback);
    return;
  }
  auto binding_info_pair_external = v8::Local<v8::External>::Cast(data);
  auto binding_info_pair =
      reinterpret_cast<BindingPair*>(binding_info_pair_external->Value());
  FunctionBindingIoProto function_invocation_proto;
  if (!V8TypesToProto(info, function_invocation_proto)) {
    isolate->ThrowError(kCouldNotConvertJsFunctionInputToNative);
    return;
  }

  // Read the request ID from the global object in the context
  const auto& request_id_label =
      TypeConverter<std::string>::ToV8(isolate, kMetadataRomaRequestId)
          .As<v8::String>();
  const auto& roma_request_id_maybe =
      context->Global()->Get(context, request_id_label);
  v8::Local<v8::Value> roma_request_id;
  std::string roma_request_id_native;
  if (roma_request_id_maybe.ToLocal(&roma_request_id) &&
      TypeConverter<std::string>::FromV8(isolate, roma_request_id,
                                         &roma_request_id_native)) {
    // Set the request ID in the function call metadata so that it is accessible
    // when the function is invoked in the user-provided binding.
    (*function_invocation_proto.mutable_metadata())[kMetadataRomaRequestId] =
        roma_request_id_native;
  } else {
    LOG(ERROR) << "Could not read request ID from metadata in hook.";
  }

  const std::string& native_function_name = binding_info_pair->first;
  if (native_function_name.empty()) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    return;
  }
  const auto result = binding_info_pair->second->function_invoker_->Invoke(
      native_function_name, function_invocation_proto);
  if (!result.Successful()) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    return;
  }
  if (!function_invocation_proto.errors().empty()) {
    isolate->ThrowError(kErrorInFunctionBindingInvocation);
    return;
  }
  const auto& returned_value =
      ProtoToV8Type(isolate, function_invocation_proto);
  info.GetReturnValue().Set(returned_value);
}

ExecutionResult V8IsolateFunctionBinding::BindFunctions(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate>& global_object_template) noexcept {
  if (!isolate) {
    return FailureExecutionResult(
        SC_ROMA_V8_ISOLATE_VISITOR_FUNCTION_BINDING_INVALID_ISOLATE);
  }
  for (const auto& binding_ref : binding_references_) {
    // Store a pointer to this V8IsolateFunctionBinding instance
    const v8::Local<v8::External>& binding_context_pair =
        v8::External::New(isolate, reinterpret_cast<void*>(binding_ref.get()));
    // Create the function template to register in the global object
    const auto function_template = v8::FunctionTemplate::New(
        isolate, &GlobalV8FunctionCallback, binding_context_pair);
    // Convert the function binding name to a v8 type
    const auto& binding_name =
        TypeConverter<std::string>::ToV8(isolate, binding_ref->first)
            .As<v8::String>();
    global_object_template->Set(binding_name, function_template);
  }
  return SuccessExecutionResult();
}

void V8IsolateFunctionBinding::AddExternalReferences(
    std::vector<intptr_t>& external_references) noexcept {
  // Must add pointers that are not within the v8 heap to external_references_
  // so that the snapshot serialization works.
  for (const auto& binding_ref : binding_references_) {
    external_references.push_back(
        reinterpret_cast<intptr_t>(binding_ref.get()));
  }
  external_references.push_back(
      reinterpret_cast<intptr_t>(&GlobalV8FunctionCallback));
}
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
