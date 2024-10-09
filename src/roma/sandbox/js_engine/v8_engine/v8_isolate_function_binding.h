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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_FUNCTION_BINDING_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_FUNCTION_BINDING_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include "absl/base/nullability.h"
#include "include/v8.h"
#include "src/roma/native_function_grpc_server/proto/callback_service.grpc.pb.h"
#include "src/roma/native_function_grpc_server/proto/callback_service.pb.h"
#include "src/roma/sandbox/native_function_binding/native_function_invoker.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {
class V8IsolateFunctionBinding {
 public:
  /**
   * @brief Create a V8IsolateFunctionBinding instance
   * @param function_names is a list of the names of the functions that can be
   * registered in the v8 context.
   */
  V8IsolateFunctionBinding(
      const std::vector<std::string>& function_names,
      const std::vector<std::string>& rpc_method_names,
      std::unique_ptr<native_function_binding::NativeFunctionInvoker>
          function_invoker,
      std::string_view server_address);

  // Not copyable or movable
  V8IsolateFunctionBinding(const V8IsolateFunctionBinding&) = delete;
  V8IsolateFunctionBinding& operator=(const V8IsolateFunctionBinding&) = delete;

  // Returns success
  bool BindFunctions(absl::Nonnull<v8::Isolate*> isolate,
                     v8::Local<v8::ObjectTemplate>& global_object_template);

  void AddExternalReferences(std::vector<intptr_t>& external_references);

  void AddIds(std::string_view uuid, std::string_view id);

  void SetMinLogLevel(absl::LogSeverity severity);

  absl::Status InvokeRpc(google::scp::roma::proto::RpcWrapper& rpc_proto);

 private:
  struct Binding {
    std::string function_name;
    V8IsolateFunctionBinding* instance;
    void (*callback)(const v8::FunctionCallbackInfo<v8::Value>&);
  };
  static void GlobalV8FunctionCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  static void GrpcServerCallback(
      const v8::FunctionCallbackInfo<v8::Value>& info);

  static bool NativeFieldsToProto(const Binding& binding,
                                  proto::FunctionBindingIoProto& function_proto,
                                  proto::RpcWrapper& rpc_proto);

  void BindFunction(
      v8::Isolate* isolate,
      v8::Local<v8::ObjectTemplate>& global_object_template, void* binding,
      void (*callback)(const v8::FunctionCallbackInfo<v8::Value>&),
      std::string_view function_name,
      absl::flat_hash_map<std::string, v8::Local<v8::ObjectTemplate>>&
          child_templates);

  std::vector<Binding> binding_references_;
  std::string invocation_req_uuid_;
  std::string invocation_req_id_;
  absl::LogSeverity min_log_level_;
  const std::vector<std::string> function_names_;
  std::unique_ptr<native_function_binding::NativeFunctionInvoker>
      function_invoker_;
  std::shared_ptr<grpc::Channel> grpc_channel_;
  std::unique_ptr<privacy_sandbox::server_common::JSCallbackService::Stub>
      stub_;
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_ISOLATE_FUNCTION_BINDING_H_
