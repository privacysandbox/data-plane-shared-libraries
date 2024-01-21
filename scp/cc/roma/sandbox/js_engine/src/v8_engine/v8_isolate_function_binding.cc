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
#include <string_view>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "roma/config/src/type_converter.h"
#include "roma/logging/src/logging.h"
#include "roma/sandbox/constants/constants.h"
#include "scp/cc/roma/interface/function_binding_io.pb.h"
#include "scp/cc/roma/sandbox/native_function_binding/src/rpc_wrapper.pb.h"

using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::proto::RpcWrapper;
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;
using Callback = void (*)(const v8::FunctionCallbackInfo<v8::Value>& info);

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

absl::LogSeverity GetSeverity(std::string_view severity) {
  if (severity == "log") {
    return absl::LogSeverity::kInfo;
  } else if (severity == "warn") {
    return absl::LogSeverity::kWarning;
  } else {
    return absl::LogSeverity::kError;
  }
}

v8::Local<v8::ObjectTemplate> CreateObjectAttribute(
    v8::Isolate* isolate, v8::Local<v8::ObjectTemplate>& object_template,
    std::string_view js_name) {
  auto obj = v8::ObjectTemplate::New(isolate);
  object_template->Set(isolate, js_name.data(), obj);
  return obj;
}

v8::Local<v8::ObjectTemplate> InitObjectJsApi(
    v8::Isolate* isolate, v8::Local<v8::ObjectTemplate>& object,
    const absl::flat_hash_map<std::string_view, Callback>&
        name_to_callback_map) {
  for (const auto& [name, callback] : name_to_callback_map) {
    auto binding_name =
        TypeConverter<std::string>::ToV8(isolate, std::string(name))
            .As<v8::String>();
    auto function_template =
        v8::FunctionTemplate::New(isolate, callback, binding_name);
    // Convert the function binding name to a v8 type
    object->Set(binding_name, function_template);
  }
  return object;
}

}  // namespace

void V8IsolateFunctionBinding::V8LogCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  v8::Isolate* isolate = info.GetIsolate();

  const auto get_input = [&info, &isolate]() {
    std::string input;
    for (int i = 0; i < info.Length(); i++) {
      absl::StrAppend(&input, *v8::String::Utf8Value(isolate, info[i]));
    }
    return input;
  };

  v8::String::Utf8Value severity(isolate, info.Data());
  LOG(LEVEL(GetSeverity(*severity))) << get_input();
}

void V8IsolateFunctionBinding::V8MetricsCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  using StrMap = absl::flat_hash_map<std::string, std::string>;
  v8::Isolate* isolate = info.GetIsolate();

  StrMap native_metrics;
  if (TypeConverter<StrMap>::FromV8Object(isolate, info[0], &native_metrics)) {
    for (const auto& metric : native_metrics) {
      LOG(INFO) << metric.first << ": " << metric.second;
    }
  }
}

bool V8IsolateFunctionBinding::NativeFieldsToProto(
    const BindingPair& binding_pair, FunctionBindingIoProto& function_proto,
    RpcWrapper& rpc_proto) {
  std::string_view native_function_name = binding_pair.first;
  if (native_function_name.empty()) {
    return false;
  }

  rpc_proto.set_function_name(native_function_name);
  rpc_proto.set_request_id(binding_pair.second->invocation_req_id_);
  rpc_proto.set_request_uuid(binding_pair.second->invocation_req_uuid_);
  *rpc_proto.mutable_io_proto() = function_proto;
  return true;
}

void V8IsolateFunctionBinding::GlobalV8FunctionCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ROMA_VLOG(9) << "Calling V8 function callback";
  auto isolate = info.GetIsolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  auto data = info.Data();
  if (data.IsEmpty()) {
    isolate->ThrowError(kUnexpectedDataInBindingCallback);
    ROMA_VLOG(1) << kUnexpectedDataInBindingCallback;
    return;
  }
  auto binding_info_pair_external = v8::Local<v8::External>::Cast(data);
  auto binding_info_pair =
      reinterpret_cast<BindingPair*>(binding_info_pair_external->Value());
  FunctionBindingIoProto function_invocation_proto;
  if (!V8TypesToProto(info, function_invocation_proto)) {
    isolate->ThrowError(kCouldNotConvertJsFunctionInputToNative);
    ROMA_VLOG(1) << kCouldNotConvertJsFunctionInputToNative;
    return;
  }

  RpcWrapper rpc_proto;
  if (!NativeFieldsToProto(*binding_info_pair, function_invocation_proto,
                           rpc_proto)) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    ROMA_VLOG(1) << kCouldNotRunFunctionBinding;
  }

  const auto result =
      binding_info_pair->second->function_invoker_->Invoke(rpc_proto);
  if (!result.ok()) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    ROMA_VLOG(1) << kCouldNotRunFunctionBinding;
    return;
  }
  if (!rpc_proto.io_proto().errors().empty()) {
    isolate->ThrowError(kErrorInFunctionBindingInvocation);
    ROMA_VLOG(1) << kErrorInFunctionBindingInvocation;
    return;
  }
  const auto& returned_value = ProtoToV8Type(isolate, rpc_proto.io_proto());
  info.GetReturnValue().Set(returned_value);
}

bool V8IsolateFunctionBinding::BindFunctions(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate>& global_object_template) noexcept {
  if (!isolate) {
    return false;
  }
  for (auto& binding_ref : binding_references_) {
    // Store a pointer to this V8IsolateFunctionBinding instance
    const v8::Local<v8::External>& binding_context_pair =
        v8::External::New(isolate, reinterpret_cast<void*>(&binding_ref));
    // Create the function template to register in the global object
    const auto function_template = v8::FunctionTemplate::New(
        isolate, &GlobalV8FunctionCallback, binding_context_pair);
    // Convert the function binding name to a v8 type
    const auto& binding_name =
        TypeConverter<std::string>::ToV8(isolate, binding_ref.first)
            .As<v8::String>();
    global_object_template->Set(binding_name, function_template);
  }

  CreateGlobalRomaObject(isolate, global_object_template);
  return true;
}

void V8IsolateFunctionBinding::CreateGlobalRomaObject(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate>& global_object_template) const {
  static auto roma =
      CreateObjectAttribute(isolate, global_object_template, "roma");
  [[maybe_unused]] static auto obj =
      InitObjectJsApi(isolate, roma,
                      absl::flat_hash_map<std::string_view, Callback>{
                          {"recordMetrics", &V8MetricsCallback},
                          {"log", &V8LogCallback},
                          {"warn", &V8LogCallback},
                          {"error", &V8LogCallback},
                      });
}

void V8IsolateFunctionBinding::AddIds(std::string_view uuid,
                                      std::string_view id) noexcept {
  if (binding_references_.size() > 0) {
    binding_references_[0].second->invocation_req_uuid_ = uuid;
    binding_references_[0].second->invocation_req_id_ = id;
  }
}

void V8IsolateFunctionBinding::AddExternalReferences(
    std::vector<intptr_t>& external_references) noexcept {
  // Must add pointers that are not within the v8 heap to external_references_
  // so that the snapshot serialization works.
  for (const auto& binding_ref : binding_references_) {
    external_references.push_back(reinterpret_cast<intptr_t>(&binding_ref));
  }
  external_references.push_back(
      reinterpret_cast<intptr_t>(&GlobalV8FunctionCallback));
  external_references.push_back(reinterpret_cast<intptr_t>(&V8LogCallback));
  external_references.push_back(reinterpret_cast<intptr_t>(&V8MetricsCallback));
}

absl::Status V8IsolateFunctionBinding::InvokeRpc(RpcWrapper& rpc_proto) {
  return function_invoker_->Invoke(rpc_proto);
}
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
