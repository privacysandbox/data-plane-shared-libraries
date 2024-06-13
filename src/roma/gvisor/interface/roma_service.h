/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_SERVICE_H_
#define SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_SERVICE_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <google/protobuf/message_lite.h>

#include "absl/log/check.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/gvisor/config/config.h"
#include "src/roma/gvisor/config/utils.h"
#include "src/roma/gvisor/host/native_function_handler.h"
#include "src/roma/gvisor/interface/roma_api.pb.h"
#include "src/roma/gvisor/interface/roma_gvisor.h"
#include "src/roma/gvisor/interface/roma_interface.h"
#include "src/roma/gvisor/interface/roma_local.h"
#include "src/roma/interface/roma.h"
#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::gvisor {

using google::scp::roma::FunctionBindingObjectV2;
using google::scp::roma::metadata_storage::MetadataStorage;

enum class Mode {
  kModeGvisor = 0,
  kModeLocal = 1,
};

template <typename TMetadata = ::google::scp::roma::DefaultMetadata>
class RomaService final {
 public:
  static absl::StatusOr<std::unique_ptr<RomaService<TMetadata>>> Create(
      Config config, Mode mode = Mode::kModeGvisor) {
    std::unique_ptr<RomaInterface> roma_interface;
    ConfigInternal config_internal;
    PS_ASSIGN_OR_RETURN(config_internal.server_socket,
                        CreateUniqueSocketName());
    PS_ASSIGN_OR_RETURN(config_internal.callback_socket,
                        CreateUniqueSocketName());
    PS_ASSIGN_OR_RETURN(config_internal.prog_dir, CreateUniqueDirectory());
    if (mode == Mode::kModeGvisor) {
      PS_ASSIGN_OR_RETURN(roma_interface,
                          RomaGvisor::Create(config, config_internal));
    } else {
      PS_ASSIGN_OR_RETURN(roma_interface,
                          RomaLocal::Create(config, config_internal));
    }
    return absl::WrapUnique(new RomaService(std::move(roma_interface),
                                            std::move(config.function_bindings),
                                            config_internal.callback_socket));
  }

  absl::StatusOr<std::string> LoadBinary(std::string_view code_path) {
    return roma_interface_->LoadBinary(code_path);
  }

  absl::Status ExecuteBinary(absl::Notification& notif,
                             std::string_view code_token,
                             const ::google::protobuf::MessageLite& request,
                             ::google::protobuf::MessageLite& response,
                             TMetadata metadata) {
    std::string request_id = google::scp::core::common::ToString(
        google::scp::core::common::Uuid::GenerateUuid());
    PS_RETURN_IF_ERROR(metadata_storage_.Add(request_id, metadata));
    ExecuteBinaryRequest exec_request;
    exec_request.set_code_token(code_token);
    exec_request.set_serialized_request(request.SerializeAsString());
    exec_request.set_request_id(request_id);
    PS_ASSIGN_OR_RETURN(ExecuteBinaryResponse exec_response,
                        roma_interface_->ExecuteBinary(exec_request));
    response.ParseFromString(exec_response.serialized_response());
    notif.Notify();
    PS_RETURN_IF_ERROR(metadata_storage_.Delete(request_id));
    return absl::OkStatus();
  }

 private:
  explicit RomaService(
      std::unique_ptr<RomaInterface> roma_interface,
      std::vector<FunctionBindingObjectV2<TMetadata>> function_bindings,
      std::string callback_socket) {
    roma_interface_ = std::move(roma_interface);
    native_function_handler_ =
        std::make_unique<NativeFunctionHandler<TMetadata>>(
            std::move(function_bindings), callback_socket, &metadata_storage_);
  }

  // Map of invocation request uuid to associated metadata.
  ::google::scp::roma::metadata_storage::MetadataStorage<TMetadata>
      metadata_storage_;
  std::unique_ptr<RomaInterface> roma_interface_;
  std::unique_ptr<NativeFunctionHandler<TMetadata>> native_function_handler_;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_SERVICE_H_
