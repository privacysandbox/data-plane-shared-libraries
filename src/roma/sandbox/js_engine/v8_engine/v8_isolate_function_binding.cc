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
#include "absl/strings/str_split.h"
#include "include/v8.h"
#include "src/roma/config/type_converter.h"
#include "src/roma/interface/function_binding_io.pb.h"
#include "src/roma/logging/logging.h"
#include "src/roma/native_function_grpc_server/interface.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/js_engine/v8_engine/performance_now.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"
#include "src/util/duration.h"

using google::scp::roma::proto::FunctionBindingIoProto;
using google::scp::roma::proto::RpcWrapper;
using google::scp::roma::sandbox::constants::kRequestId;
using google::scp::roma::sandbox::constants::kRequestUuid;
using Callback = void (*)(const v8::FunctionCallbackInfo<v8::Value>& info);

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
namespace {
constexpr char kCouldNotRunFunctionBinding[] =
    "ROMA: Could not run C++ function binding.";
constexpr char kUnexpectedDataInBindingCallback[] =
    "ROMA: Unexpected data in global callback.";
constexpr char kUnexpectedParametersInBindingCallback[] =
    "ROMA: Unexpected parameters in global callback.";
constexpr char kUnexpectedParameterTypeInBindingCallback[] =
    "ROMA: Unexpected parameter type in global callback.";
constexpr char kCouldNotConvertJsFunctionInputToNative[] =
    "ROMA: Could not convert JS function input to native C++ type.";
constexpr char kErrorInFunctionBindingInvocation[] =
    "ROMA: Error while executing native function binding.";

v8::Local<v8::ObjectTemplate> CreatePerformanceTemplate(v8::Isolate* isolate) {
  v8::Local<v8::ObjectTemplate> performance_template =
      v8::ObjectTemplate::New(isolate);
  performance_template->Set(isolate, "now",
                            v8::FunctionTemplate::New(isolate, PerformanceNow));
  performance_template->Set(isolate, "timeOrigin",
                            GetPerformanceStartTime(isolate));
  return performance_template;
}

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

bool ShouldAbortLoggingCallback(std::string_view function_name,
                                absl::LogSeverity min_log_level) {
  if (function_name == "ROMA_ERROR") {
    return min_log_level > absl::LogSeverity::kError;
  } else if (function_name == "ROMA_WARN") {
    return min_log_level > absl::LogSeverity::kWarning;
  } else if (function_name == "ROMA_LOG") {
    return min_log_level > absl::LogSeverity::kInfo;
  } else {
    return false;  // not a logging callback at all, do not abort
  }
}

}  // namespace

V8IsolateFunctionBinding::V8IsolateFunctionBinding(
    const std::vector<std::string>& function_names,
    const std::vector<std::string>& rpc_method_names,
    std::unique_ptr<native_function_binding::NativeFunctionInvoker>
        function_invoker,
    std::string_view server_address)
    : function_invoker_(std::move(function_invoker)) {
  for (const auto& function_name : function_names) {
    binding_references_.emplace_back(Binding{
        .function_name = function_name,
        .instance = this,
        .callback = &GlobalV8FunctionCallback,
    });
  }
  for (const auto& rpc_method_name : rpc_method_names) {
    binding_references_.emplace_back(Binding{
        .function_name = rpc_method_name,
        .instance = this,
        .callback = &GrpcServerCallback,
    });
  }

  if (!server_address.empty()) {
    grpc_channel_ = grpc::CreateChannel(std::string(server_address),
                                        grpc::InsecureChannelCredentials());
    stub_ = privacy_sandbox::server_common::JSCallbackService::NewStub(
        grpc_channel_);
  }
}

bool V8IsolateFunctionBinding::NativeFieldsToProto(
    const Binding& binding, FunctionBindingIoProto& function_proto,
    RpcWrapper& rpc_proto) {
  std::string_view native_function_name = binding.function_name;
  if (native_function_name.empty()) {
    return false;
  }

  rpc_proto.set_function_name(native_function_name);
  rpc_proto.set_request_id(binding.instance->invocation_req_id_);
  rpc_proto.set_request_uuid(binding.instance->invocation_req_uuid_);
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
  auto binding_external = v8::Local<v8::External>::Cast(data);
  auto binding = reinterpret_cast<Binding*>(binding_external->Value());
  FunctionBindingIoProto function_invocation_proto;
  if (!V8TypesToProto(info, function_invocation_proto)) {
    isolate->ThrowError(kCouldNotConvertJsFunctionInputToNative);
    ROMA_VLOG(1) << kCouldNotConvertJsFunctionInputToNative;
    return;
  }

  RpcWrapper rpc_proto;
  if (!NativeFieldsToProto(*binding, function_invocation_proto, rpc_proto)) {
    isolate->ThrowError(kCouldNotRunFunctionBinding);
    ROMA_VLOG(1) << kCouldNotRunFunctionBinding;
  }

  if (ShouldAbortLoggingCallback(rpc_proto.function_name(),
                                 binding->instance->min_log_level_)) {
    return;
  }

  const auto result = binding->instance->function_invoker_->Invoke(rpc_proto);
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

void V8IsolateFunctionBinding::GrpcServerCallback(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  ROMA_VLOG(9) << "Calling V8 gRPC Server callback";
  auto isolate = info.GetIsolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  auto data = info.Data();
  if (data.IsEmpty()) {
    isolate->ThrowError(kUnexpectedDataInBindingCallback);
    ROMA_VLOG(1) << kUnexpectedDataInBindingCallback;
    return;
  }

  if (info.Length() != 1) {
    isolate->ThrowError(kUnexpectedParametersInBindingCallback);
    ROMA_VLOG(1) << kUnexpectedParametersInBindingCallback;
    return;
  }

  std::string native_data;
  if (!TypeConverter<std::string>::FromV8(isolate, info[0], &native_data)) {
    // Handle error: Value is not backed by an v8::String
    isolate->ThrowError(kUnexpectedParameterTypeInBindingCallback);
    ROMA_VLOG(1) << kUnexpectedParameterTypeInBindingCallback;
    return;
  }

  auto binding_external = v8::Local<v8::External>::Cast(data);
  auto binding = reinterpret_cast<Binding*>(binding_external->Value());
  auto& stub = binding->instance->stub_;
  auto uuid = binding->instance->invocation_req_uuid_;

  privacy_sandbox::server_common::InvokeCallbackRequest request;
  request.set_function_name(binding->function_name);
  *request.mutable_request_payload() = std::move(native_data);

  privacy_sandbox::server_common::InvokeCallbackResponse response;
  grpc::ClientContext context;
  context.AddMetadata(std::string(google::scp::roma::grpc_server::kUuidTag),
                      uuid);

  const grpc::Status status =
      stub->InvokeCallback(&context, request, &response);

  if (status.ok()) {
    info.GetReturnValue().Set(
        TypeConverter<std::string>::ToV8(isolate, response.response_payload()));
  } else {
    info.GetReturnValue().Set(
        TypeConverter<std::string>::ToV8(isolate, status.error_message()));
  }
}

void V8IsolateFunctionBinding::BindFunction(
    v8::Isolate* isolate, v8::Local<v8::ObjectTemplate>& global_object_template,
    void* binding, void (*callback)(const v8::FunctionCallbackInfo<v8::Value>&),
    std::string_view function_name,
    absl::flat_hash_map<std::string, v8::Local<v8::ObjectTemplate>>&
        child_templates) {
  const v8::Local<v8::External>& binding_context =
      v8::External::New(isolate, binding);

  std::vector<std::string> tokens = absl::StrSplit(function_name, ".");
  // To support nested child objects with the same name
  std::string prefix = "";
  v8::Local<v8::ObjectTemplate> current_template = global_object_template;
  for (int i = 0; i < tokens.size() - 1; i++) {
    absl::StrAppend(&prefix, tokens[i], "/");
    // Try to create a new ObjectTemplate for `prefix` in `child_templates`
    auto [it, inserted] =
        child_templates.try_emplace(prefix, v8::ObjectTemplate::New(isolate));
    // If a ObjectTemplate for `prefix` doesn't already exist (If the child
    // object hasn't already been created on the current object), add the newly
    // created ObjectTemplate to `current_template`
    if (inserted) {
      const auto& object_binding_name =
          TypeConverter<std::string>::ToV8(isolate, tokens[i]).As<v8::String>();
      current_template->Set(object_binding_name, it->second);
    }
    current_template = it->second;
  }

  const auto function_template =
      v8::FunctionTemplate::New(isolate, callback, binding_context);
  const auto& binding_name =
      TypeConverter<std::string>::ToV8(isolate, tokens.back()).As<v8::String>();
  current_template->Set(binding_name, function_template);
}

// BindFunctions does not guarantee thread safety, but it is not a requirement.
bool V8IsolateFunctionBinding::BindFunctions(
    absl::Nonnull<v8::Isolate*> isolate,
    v8::Local<v8::ObjectTemplate>& global_object_template) {
  if (!isolate) {
    return false;
  }
  absl::flat_hash_map<std::string, v8::Local<v8::ObjectTemplate>>
      child_templates;
  for (auto& binding : binding_references_) {
    BindFunction(isolate, global_object_template,
                 reinterpret_cast<void*>(&binding), binding.callback,
                 binding.function_name, child_templates);
  }
  const auto& binding_name =
      TypeConverter<std::string>::ToV8(isolate, "performance").As<v8::String>();
  global_object_template->Set(binding_name, CreatePerformanceTemplate(isolate));
  return true;
}

void V8IsolateFunctionBinding::AddIds(std::string_view uuid,
                                      std::string_view id) {
  if (!binding_references_.empty()) {
    // All binding_references_ share a single V8IsolateFunctionBinding instance,
    // so setting the members on binding_references[0].instance changes all
    // elements of binding_references_.
    binding_references_[0].instance->invocation_req_uuid_ = uuid;
    binding_references_[0].instance->invocation_req_id_ = id;
  }
}

void V8IsolateFunctionBinding::AddExternalReferences(
    std::vector<intptr_t>& external_references) {
  // Must add pointers that are not within the v8 heap to external_references_
  // so that the snapshot serialization works.
  for (const auto& binding : binding_references_) {
    external_references.push_back(reinterpret_cast<intptr_t>(&binding));
  }
  external_references.push_back(
      reinterpret_cast<intptr_t>(&GlobalV8FunctionCallback));
  external_references.push_back(
      reinterpret_cast<intptr_t>(&GrpcServerCallback));
  external_references.push_back(reinterpret_cast<intptr_t>(&PerformanceNow));
}

absl::Status V8IsolateFunctionBinding::InvokeRpc(RpcWrapper& rpc_proto) {
  return function_invoker_->Invoke(rpc_proto);
}

void V8IsolateFunctionBinding::SetMinLogLevel(absl::LogSeverity severity) {
  min_log_level_ = severity;
}
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
