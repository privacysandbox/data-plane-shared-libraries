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

#ifndef SRC_ROMA_GVISOR_INTERFACE_ROMA_SERVICE_H_
#define SRC_ROMA_GVISOR_INTERFACE_ROMA_SERVICE_H_

#include <stdio.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
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

#include "absl/container/flat_hash_map.h"
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
#include "src/util/execution_token.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::byob {

namespace internal::roma_service {

class LocalHandle final {
 public:
  LocalHandle(int pid, std::string_view mounts, std::string_view socket_path,
              std::string_view log_dir);
  ~LocalHandle();

 private:
  int pid_;
};

class ByobHandle final {
 public:
  ByobHandle(int pid, std::string_view mounts, std::string_view socket_path,
             std::string_view sockdir, std::string container_name,
             std::string_view log_dir, std::uint64_t memory_limit_soft,
             std::uint64_t memory_limit_hard, bool debug_mode);
  ~ByobHandle();

 private:
  int pid_;
  std::string container_name_;
};

}  // namespace internal::roma_service

enum class Mode {
  kModeSandbox,
  kModeNoSandbox,
  kModeSandboxDebug,
};

inline bool AbslParseFlag(absl::string_view text, Mode* mode,
                          std::string* error) {
  if (text == "on") {
    *mode = Mode::kModeSandbox;
    return true;
  }
  if (text == "debug") {
    *mode = Mode::kModeSandboxDebug;
    return true;
  }
  if (text == "off") {
    *mode = Mode::kModeNoSandbox;
    return true;
  }
  *error = "Supported values: on, off, debug.";
  return false;
}

inline std::string AbslUnparseFlag(Mode mode) {
  switch (mode) {
    case Mode::kModeSandbox:
      return "on";
    case Mode::kModeSandboxDebug:
      return "debug";
    case Mode::kModeNoSandbox:
      return "off";
    default:
      return absl::StrCat(mode);
  }
}

template <typename TMetadata = google::scp::roma::DefaultMetadata>
class RomaService final {
 public:
  absl::Status Init(Config<TMetadata> config, Mode mode) {
    char socket_dir_tmpl[20] = "/tmp/sockdir_XXXXXX";
    if (::mkdtemp(socket_dir_tmpl) == nullptr) {
      return absl::ErrnoToStatus(errno, "mkdtemp(socket_dir)");
    }
    socket_dir_ = socket_dir_tmpl;
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
      return absl::ErrnoToStatus(errno, "socket()");
    }
    std::filesystem::path socket_path = socket_dir_ / "byob_rpc.sock";
    {
      ::sockaddr_un sa = {
          .sun_family = AF_UNIX,
      };
      socket_path.string().copy(sa.sun_path, sizeof(sa.sun_path));
      if (::bind(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == -1) {
        return absl::ErrnoToStatus(errno, "bind()");
      }
      if (::listen(fd, /*backlog=*/4096) == -1) {
        return absl::ErrnoToStatus(errno, "listen()");
      }
    }
    std::filesystem::permissions(socket_dir_,
                                 std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_all |
                                     std::filesystem::perms::others_all);
    std::filesystem::permissions(socket_path,
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::group_read |
                                     std::filesystem::perms::others_read |
                                     std::filesystem::perms::owner_write |
                                     std::filesystem::perms::group_write |
                                     std::filesystem::perms::others_write);
    char log_dir_tmpl[20] = "/tmp/log_dir_XXXXXX";
    if (::mkdtemp(log_dir_tmpl) == nullptr) {
      return absl::ErrnoToStatus(errno, "mkdtemp(log_dir)");
    }
    log_dir_ = log_dir_tmpl;
    std::filesystem::permissions(log_dir_,
                                 std::filesystem::perms::owner_all |
                                     std::filesystem::perms::group_all |
                                     std::filesystem::perms::others_all);
    // Need to set this to ensure all the children can be reaped
    ::prctl(PR_SET_CHILD_SUBREAPER, 1);
    const int pid = ::fork();
    if (pid == -1) {
      return absl::ErrnoToStatus(errno, "fork()");
    }
    switch (mode) {
      case Mode::kModeSandbox:
      case Mode::kModeSandboxDebug:
        handle_.emplace<internal::roma_service::ByobHandle>(
            pid, config.lib_mounts, socket_path.c_str(), socket_dir_.c_str(),
            std::move(config.roma_container_name), log_dir_.c_str(),
            config.memory_limit_soft, config.memory_limit_hard,
            /*debug=*/mode == Mode::kModeSandboxDebug);
        break;
      case Mode::kModeNoSandbox:
        handle_.emplace<internal::roma_service::LocalHandle>(
            pid, config.lib_mounts, socket_path.c_str(), log_dir_.c_str());
        break;
      default:
        return absl::InternalError("Unsupported mode in switch");
    }
    dispatcher_.emplace();
    PS_RETURN_IF_ERROR(dispatcher_->Init(fd, log_dir_));
    function_bindings_.reserve(config.function_bindings.size());
    for (auto& binding : config.function_bindings) {
      function_bindings_[std::move(binding.function_name)] =
          std::move(binding.function);
    }
    return absl::OkStatus();
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
      std::string_view code_token, Request request, TMetadata metadata,
      absl::AnyInvocable<void(absl::StatusOr<Response>,
                              absl::StatusOr<std::string_view> logs) &&>
          callback) {
    return dispatcher_->ProcessRequest(code_token, std::move(request),
                                       std::move(metadata), function_bindings_,
                                       std::move(callback));
  }

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, Request request, TMetadata metadata,
      absl::AnyInvocable<void(absl::StatusOr<Response>) &&> callback) {
    return ProcessRequest<Response>(
        code_token, std::move(request), std::move(metadata),
        [callback = std::move(callback)](
            absl::StatusOr<Response> response,
            absl::StatusOr<std::string_view> logs) mutable {
          std::move(callback)(response);
        });
  }

  template <typename Response, typename Request>
  absl::StatusOr<google::scp::roma::ExecutionToken> ProcessRequest(
      std::string_view code_token, Request request, TMetadata metadata,
      absl::Notification& notif,
      absl::StatusOr<std::unique_ptr<Response>>& output) {
    return ProcessRequest<Response>(
        code_token, std::move(request), std::move(metadata),
        [&notif, &output](absl::StatusOr<Response> response,
                          absl::StatusOr<std::string_view> logs) {
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
               internal::roma_service::ByobHandle>
      handle_;
  std::optional<Dispatcher> dispatcher_;
  absl::flat_hash_map<
      std::string, std::function<void(
                       google::scp::roma::FunctionBindingPayload<TMetadata>&)>>
      function_bindings_;
};

}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_GVISOR_INTERFACE_ROMA_SERVICE_H_
