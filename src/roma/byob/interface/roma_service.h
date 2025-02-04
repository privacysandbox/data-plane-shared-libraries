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

class LocalHandle final {
 public:
  LocalHandle(int pid, std::string_view mounts,
              std::string_view control_socket_path,
              std::string_view udf_socket_path, std::string_view sock_dir,
              std::string_view log_dir, bool enable_seccomp_filter);
  ~LocalHandle();

 private:
  int pid_;
};

class ByobHandle final {
 public:
  ByobHandle(int pid, std::string_view mounts,
             std::string_view control_socket_path,
             std::string_view udf_socket_path, std::string_view sock_dir,
             std::string container_name, std::string_view log_dir,
             std::uint64_t memory_limit_soft, std::uint64_t memory_limit_hard,
             bool debug_mode, bool enable_seccomp_filter);
  ~ByobHandle();

 private:
  int pid_;
  std::string container_name_;
};

class NsJailHandle final {
 public:
  NsJailHandle(int pid, std::string_view mounts,
               std::string_view control_socket_path,
               std::string_view udf_socket_path, std::string_view socket_dir,
               std::string container_name, std::string_view log_dir,
               std::uint64_t memory_limit_soft, std::uint64_t memory_limit_hard,
               bool enable_seccomp_filter);
  ~NsJailHandle();

 private:
  int pid_;
};

}  // namespace internal::roma_service

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class RomaService final {
 public:
  absl::Status Init(Config<TMetadata> config, Mode mode) {
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

    const int pid = ::fork();
    if (pid == -1) {
      return absl::ErrnoToStatus(errno, "fork()");
    }
    switch (mode) {
      case Mode::kModeGvisorSandbox:
        [[fallthrough]];
      case Mode::kModeGvisorSandboxDebug:
        handle_.emplace<internal::roma_service::ByobHandle>(
            pid, config.lib_mounts, control_socket_path.c_str(),
            udf_socket_path.c_str(), socket_dir_.c_str(),
            std::move(config.roma_container_name), log_dir_.c_str(),
            config.memory_limit_soft, config.memory_limit_hard,
            /*debug=*/mode == Mode::kModeGvisorSandboxDebug,
            /*enable_seccomp_filter=*/config.enable_seccomp_filter);
        break;
      case Mode::kModeMinimalSandbox:
        handle_.emplace<internal::roma_service::LocalHandle>(
            pid, config.lib_mounts, control_socket_path.c_str(),
            udf_socket_path.c_str(), socket_dir_.c_str(), log_dir_.c_str(),
            /*enable_seccomp_filter=*/config.enable_seccomp_filter);
        break;
      case Mode::kModeNsJailSandbox:
        handle_.emplace<internal::roma_service::NsJailHandle>(
            pid, config.lib_mounts, control_socket_path.c_str(),
            udf_socket_path.c_str(), socket_dir_.c_str(),
            std::move(config.roma_container_name), log_dir_.c_str(),
            config.memory_limit_soft, config.memory_limit_hard,
            config.enable_seccomp_filter);
        break;
      default:
        return absl::InternalError("Unsupported mode in switch");
    }
    dispatcher_.emplace();
    return dispatcher_->Init(std::move(control_socket_path),
                             std::move(udf_socket_path), log_dir_);
  }

  ~RomaService() {
    dispatcher_.reset();
    handle_.emplace<std::monostate>();
    if (std::error_code ec; std::filesystem::remove_all(socket_dir_, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove dir " << socket_dir_ << ": "
                 << ec.message();
    }
    if (std::error_code ec; std::filesystem::remove_all(log_dir_, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      LOG(ERROR) << "Failed to remove dir " << log_dir_ << ": " << ec.message();
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
      TMetadata /*metadata*/,
      absl::AnyInvocable<void(absl::StatusOr<Response>,
                              absl::StatusOr<std::string_view>) &&>
          callback) {
    return dispatcher_->ProcessRequest(code_token, request,
                                       std::move(callback));
  }

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, const Request& request, TMetadata metadata,
      absl::AnyInvocable<void(absl::StatusOr<Response>) &&> callback) {
    return ProcessRequest<Response>(
        code_token, request, std::move(metadata),
        [callback = std::move(callback)](
            absl::StatusOr<Response> response,
            absl::StatusOr<std::string_view> /*logs*/) mutable {
          std::move(callback)(std::move(response));
        });
  }

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, const Request& request, TMetadata metadata,
      absl::Notification& notif,
      absl::StatusOr<std::unique_ptr<Response>>& output) {
    return ProcessRequest<Response>(
        code_token, request, std::move(metadata),
        [&notif, &output](absl::StatusOr<Response> response,
                          absl::StatusOr<std::string_view> /*logs*/) {
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
  std::variant<std::monostate, internal::roma_service::LocalHandle,
               internal::roma_service::ByobHandle,
               internal::roma_service::NsJailHandle>
      handle_;
  std::optional<Dispatcher> dispatcher_;
};

}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_INTERFACE_ROMA_SERVICE_H_
