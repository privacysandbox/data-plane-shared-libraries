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

#ifndef SRC_ROMA_BYOB_INTERFACE_ROMA_SERVICE_H_
#define SRC_ROMA_BYOB_INTERFACE_ROMA_SERVICE_H_

#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <variant>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/notification.h"
#include "absl/time/time.h"
#include "google/protobuf/any.pb.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/utility/utils.h"
#include "src/roma/config/function_binding_object_v2.h"
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::byob {

namespace internal::roma_service {

class Handle {
 public:
  struct Options {
    // `socket_dir` and `log_dir` are `string_view` because their lifetime is
    // managed by the roma_service instance rather than configured by the user.
    std::string_view socket_dir;
    std::string_view log_dir;
    std::string_view binary_dir;
    std::filesystem::path control_socket_path;
    std::filesystem::path udf_socket_path;
    std::string mounts;
    std::string container_name;
    std::uint64_t memory_limit_soft;
    std::uint64_t memory_limit_hard;
    ::privacy_sandbox::server_common::byob::SyscallFiltering syscall_filtering;
    bool disable_ipc_namespace;
    bool debug_mode;
  };
  virtual ~Handle() = default;
  static absl::StatusOr<std::unique_ptr<Handle>> CreateHandle(Mode mode,
                                                              Options options);
};

}  // namespace internal::roma_service

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class RomaService final {
 public:
  absl::Status Init(Config<TMetadata> config, Mode mode) {
    if (config.disable_ipc_namespace &&
        config.syscall_filtering == ::privacy_sandbox::server_common::byob::
                                        SyscallFiltering::kNoSyscallFiltering) {
      return absl::InvalidArgumentError(
          "Either syscall filtering OR IPC namespacing needs to be enabled");
    }
    socket_dir_ = std::filesystem::path(RUN_WORKERS_PATH) / "socket_dir";
    PS_RETURN_IF_ERROR(
        ::privacy_sandbox::server_common::byob::CreateDirectories(socket_dir_));
    std::filesystem::permissions(socket_dir_,
                                 std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_all |
                                     std::filesystem::perms::others_all);
    log_dir_ = std::filesystem::path(RUN_WORKERS_PATH) / "log_dir";
    PS_RETURN_IF_ERROR(
        ::privacy_sandbox::server_common::byob::CreateDirectories(log_dir_));
    std::filesystem::permissions(log_dir_,
                                 std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_all |
                                     std::filesystem::perms::others_all);
    std::filesystem::path control_socket_path = socket_dir_ / "control.sock";
    std::filesystem::path udf_socket_path = socket_dir_ / "byob_rpc.sock";
    binary_dir_ = std::filesystem::path(RUN_WORKERS_PATH) / "binary_dir";
    PS_RETURN_IF_ERROR(
        ::privacy_sandbox::server_common::byob::CreateDirectories(binary_dir_));
    std::filesystem::permissions(binary_dir_,
                                 std::filesystem::perms::owner_all);

    PS_ASSIGN_OR_RETURN(
        handle_,
        internal::roma_service::Handle::CreateHandle(
            mode, {
                      .socket_dir = socket_dir_.native(),
                      .log_dir = log_dir_.native(),
                      .binary_dir = binary_dir_.native(),
                      .control_socket_path = control_socket_path,
                      .udf_socket_path = udf_socket_path,
                      .mounts = std::move(config.lib_mounts),
                      .container_name = std::move(config.roma_container_name),
                      .memory_limit_soft = config.memory_limit_soft,
                      .memory_limit_hard = config.memory_limit_hard,
                      .syscall_filtering = config.syscall_filtering,
                      .disable_ipc_namespace = config.disable_ipc_namespace,
                      .debug_mode = mode == Mode::kModeGvisorSandboxDebug,
                  }));
    dispatcher_.emplace();
    return dispatcher_->Init(std::move(control_socket_path),
                             std::move(udf_socket_path), log_dir_, binary_dir_);
  }

  ~RomaService() {
    dispatcher_.reset();
    handle_.reset();
    if (std::error_code ec; std::filesystem::remove_all(socket_dir_, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove dir " << socket_dir_ << ": "
                 << ec.message();
    }
    if (std::error_code ec; std::filesystem::remove_all(log_dir_, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove dir " << log_dir_ << ": " << ec.message();
    }
    if (std::error_code ec; std::filesystem::remove_all(binary_dir_, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove dir " << binary_dir_ << ": "
                 << ec.message();
    }
  }

  absl::StatusOr<std::string> LoadBinary(std::filesystem::path code_path,
                                         int num_workers) {
    return dispatcher_->LoadBinary(std::move(code_path), num_workers);
  }

  void Delete(std::string_view code_token) { dispatcher_->Delete(code_token); }

  void Cancel(google::scp::roma::ExecutionToken token) {
    dispatcher_->Cancel(std::move(token));
  }

  absl::StatusOr<std::string> LoadBinaryForLogging(
      std::filesystem::path code_path, int num_workers) {
    return dispatcher_->LoadBinary(std::move(code_path), num_workers,
                                   /*enable_log_egress*/ true);
  }

  absl::StatusOr<std::string> LoadBinaryForLogging(
      std::string no_log_code_token, int num_workers) {
    return dispatcher_->LoadBinaryForLogging(std::move(no_log_code_token),
                                             num_workers);
  }

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, const Request& request,
      TMetadata /*metadata*/, absl::Duration connection_timeout,
      absl::AnyInvocable<void(absl::StatusOr<Response>,
                              absl::StatusOr<std::string_view>,
                              ProcessRequestMetrics) &&>
          callback) {
    return dispatcher_->ProcessRequest(code_token, request, connection_timeout,
                                       std::move(callback));
  }

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, const Request& request, TMetadata metadata,
      absl::Duration connection_timeout, absl::Notification& notif,
      absl::StatusOr<std::unique_ptr<Response>>& output) {
    return ProcessRequest<Response>(
        code_token, request, std::move(metadata), connection_timeout,
        [&notif, &output](absl::StatusOr<Response> response,
                          absl::StatusOr<std::string_view> /*logs*/,
                          ProcessRequestMetrics /*metrics*/) {
          if (response.ok()) {
            output = std::make_unique<Response>(*std::move(response));
          } else {
            output = std::move(response).status();
          }
          notif.Notify();
        });
  }

 private:
  int num_workers_;
  std::filesystem::path socket_dir_;
  std::filesystem::path log_dir_;
  std::filesystem::path binary_dir_;
  std::unique_ptr<internal::roma_service::Handle> handle_;
  std::optional<Dispatcher> dispatcher_;
};

}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_INTERFACE_ROMA_SERVICE_H_
