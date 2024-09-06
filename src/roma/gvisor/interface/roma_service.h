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

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/grpcpp.h>

#include <google/protobuf/message_lite.h>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/gvisor/config/config.h"
#include "src/roma/gvisor/config/utils.h"
#include "src/roma/gvisor/host/native_function_handler.h"
#include "src/roma/gvisor/interface/roma_api.grpc.pb.h"
#include "src/roma/gvisor/interface/roma_api.pb.h"
#include "src/roma/gvisor/interface/roma_gvisor.h"
#include "src/roma/gvisor/interface/roma_interface.h"
#include "src/roma/gvisor/interface/roma_local.h"
#include "src/roma/interface/roma.h"
#include "src/roma/metadata_storage/metadata_storage.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

namespace privacy_sandbox::server_common::gvisor {

enum class Mode {
  kModeGvisor = 0,
  kModeLocal = 1,
};

template <typename TMetadata = ::google::scp::roma::DefaultMetadata>
class RomaService final {
 public:
  static absl::StatusOr<std::unique_ptr<RomaService<TMetadata>>> Create(
      Config<TMetadata> config, Mode mode = Mode::kModeGvisor) {
    std::unique_ptr<RomaInterface> roma_interface;
    ConfigInternal config_internal;
    config_internal.num_workers = config.num_workers;
    config_internal.roma_container_name = std::move(config.roma_container_name);
    config_internal.lib_mounts = std::move(config.lib_mounts);
    PS_ASSIGN_OR_RETURN(config_internal.server_socket,
                        CreateUniqueSocketName());
    PS_ASSIGN_OR_RETURN(config_internal.callback_socket,
                        CreateUniqueSocketName());
    PS_ASSIGN_OR_RETURN(config_internal.prog_dir, CreateUniqueDirectory());
    std::shared_ptr<::grpc::Channel> channel = ::grpc::CreateChannel(
        absl::StrCat("unix://", config_internal.server_socket),
        ::grpc::InsecureChannelCredentials());
    if (mode == Mode::kModeGvisor) {
      PS_ASSIGN_OR_RETURN(roma_interface,
                          RomaGvisor::Create(config_internal, channel));
    } else {
      PS_ASSIGN_OR_RETURN(roma_interface,
                          RomaLocal::Create(config_internal, channel));
    }
    return absl::WrapUnique(new RomaService(
        std::move(roma_interface), std::move(config.function_bindings),
        config_internal.callback_socket, std::move(channel),
        config_internal.prog_dir));
  }

  /**
   * @brief Loads a new binary asynchronously from the provided code_path.
   *
   * @paragraph Once the load operation has been completed, notification will be
   * sent via absl::Notification namely notif. If load is successful, the
   * load_status will be populated with an ok status else with the error
   * status and message. If load is successful, ExecuteBinary can be called on
   * the code using the code_token returned by this function.
   *
   * @param code_path path to the binary to be loaded into the sandbox.
   * @param notif notifies that load_status is available.
   * @param load_status is populated with the status of load once load is
   * completed. If the status is ok, then code_token returned by this function
   * can be used for calling this binary in subsequent execute requests.
   * @return absl::StatusOr<std::string> returns the code_token.
   */
  absl::StatusOr<std::string> LoadBinary(std::string_view code_path,
                                         absl::Notification& notif,
                                         absl::Status& load_status) {
    std::string code_token_str = ::google::scp::core::common::ToString(
        ::google::scp::core::common::Uuid::GenerateUuid());
    PS_RETURN_IF_ERROR(CopyFile(code_path, prog_dir_, code_token_str));

    struct LoadBinaryArgs {
      ::grpc::ClientContext context;
      LoadBinaryRequest request;
      LoadBinaryResponse response;
    };

    std::shared_ptr<LoadBinaryArgs> load_args =
        std::make_shared<LoadBinaryArgs>();
    load_args->request.set_code_token(code_token_str);
    stub_->async()->LoadBinary(
        &load_args->context, &load_args->request, &load_args->response,
        // Load args are moved into the callback to extend the lifetime so that
        // they are available when needed by the async gRPC.
        [load_args = std::move(load_args), &notif,
         &load_status](::grpc::Status status) {
          load_status = ToAbslStatus(status);
          // Once load_status is populated, notify load
          // is complete and status is available.
          notif.Notify();
        });
    return code_token_str;
  }

  /**
   * @brief Executes the binary referred to by the provided code_token
   * asynchronously.
   *
   * Once the async execute is complete, notification will be sent via the
   * provided notif. If successful, the grpc::Status passed to the
   * populate_response function will indicate the same the serialized_response
   * can then be used processing the response.
   *
   * @param code_token identifier provided by load of the binary to be executed.
   * @param request serialized proto for the binary.
   * @param metadata for execution request. It is a templated type.
   * @param populate_response invoked once the response is available.
   * @param notif notifies that execution_status is available.
   * @return absl::Status
   */
  absl::Status ExecuteBinary(
      std::string_view code_token, std::string request, TMetadata metadata,
      std::function<void(::grpc::Status status,
                         const std::string& serialized_response)>
          populate_response,
      absl::Notification& notif) {
    std::string request_id = google::scp::core::common::ToString(
        google::scp::core::common::Uuid::GenerateUuid());
    PS_RETURN_IF_ERROR(metadata_storage_.Add(request_id, metadata));

    struct ExecuteBinaryArgs {
      ::grpc::ClientContext context;
      ExecuteBinaryRequest request;
      ExecuteBinaryResponse response;
    };

    std::shared_ptr<ExecuteBinaryArgs> exec_args =
        std::make_shared<ExecuteBinaryArgs>();
    exec_args->request.set_code_token(code_token);
    exec_args->request.set_serialized_request(std::move(request));
    exec_args->request.set_request_id(request_id);
    stub_->async()->ExecuteBinary(
        &exec_args->context, &exec_args->request, &exec_args->response,
        // Exec args are moved into the callback to extend the lifetime so that
        // they are available when needed by the async gRPC.
        [&, exec_args = std::move(exec_args),
         request_id = std::move(request_id),
         populate_response =
             std::move(populate_response)](::grpc::Status status) {
          populate_response(std::move(status),
                            exec_args->response.serialized_response());
          // Once response and execution_status have been populated, notify that
          // execution has been completed.
          notif.Notify();
          // Post-execution cleanup.
          // Delete metadata from storage.
          if (!metadata_storage_.Delete(request_id).ok()) {
            LOG(ERROR) << "Failed to delete metadata for request " << request_id
                       << " from metadata storage";
          }
        });
    return absl::OkStatus();
  }

 private:
  explicit RomaService(
      std::unique_ptr<RomaInterface> roma_interface,
      std::vector<::google::scp::roma::FunctionBindingObjectV2<TMetadata>>
          function_bindings,
      std::string callback_socket, std::shared_ptr<::grpc::Channel> channel,
      std::string prog_dir) {
    roma_interface_ = std::move(roma_interface);
    stub_ = RomaGvisorService::NewStub(channel);
    prog_dir_ = prog_dir;
    native_function_handler_ =
        std::make_unique<NativeFunctionHandler<TMetadata>>(
            std::move(function_bindings), callback_socket, &metadata_storage_);
  }

  // Map of invocation request uuid to associated metadata.
  ::google::scp::roma::metadata_storage::MetadataStorage<TMetadata>
      metadata_storage_;
  std::unique_ptr<RomaGvisorService::Stub> stub_;
  std::string prog_dir_;
  std::unique_ptr<RomaInterface> roma_interface_;
  std::unique_ptr<NativeFunctionHandler<TMetadata>> native_function_handler_;
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_INTERFACE_ROMA_GVISOR_SERVICE_H_
