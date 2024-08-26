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
  LocalHandle(int pid, std::string_view mounts, std::string_view progdir,
              std::string_view socket_name);
  ~LocalHandle();

 private:
  int pid_;
};

class ByobHandle final {
 public:
  ByobHandle(int pid, std::string_view mounts, std::string_view progdir,
             std::string_view socket_name, std::string_view sockdir,
             std::string container_name);
  ~ByobHandle();

 private:
  int pid_;
  std::string container_name_;
};
}  // namespace internal::roma_service

enum class Mode {
  kModeSandbox = 0,
  kModeNoSandbox = 1,
};

template <typename TMetadata = ::google::scp::roma::DefaultMetadata>
class RomaService final {
 public:
  absl::Status Init(Config<TMetadata> config, Mode mode) {
    n_workers_ = config.num_workers > 0
                     ? config.num_workers
                     : static_cast<int>(std::thread::hardware_concurrency());
    if (::mkdtemp(progdir_) == nullptr) {
      return absl::ErrnoToStatus(errno, "mkdtemp(\"/tmp/progdir_XXXXXX\")");
    }
    if (::mkdtemp(sockdir_) == nullptr) {
      return absl::ErrnoToStatus(errno, "mkdtemp(\"/tmp/sockdir_XXXXXX\")");
    }
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd == -1) {
      return absl::ErrnoToStatus(errno, "socket()");
    }
    socket_name_ = ::tempnam(sockdir_, nullptr);
    if (socket_name_ == nullptr) {
      return absl::ErrnoToStatus(errno, "tempnam()");
    }
    {
      sockaddr_un sa;
      ::memset(&sa, 0, sizeof(sa));
      sa.sun_family = AF_UNIX;
      ::strncpy(sa.sun_path, socket_name_, sizeof(sa.sun_path));
      if (::bind(fd, reinterpret_cast<sockaddr*>(&sa), SUN_LEN(&sa)) == -1) {
        return absl::ErrnoToStatus(errno, "bind()");
      }
      if (::listen(fd, /*backlog=*/0) == -1) {
        return absl::ErrnoToStatus(errno, "listen()");
      }
    }
    // Need to set this to ensure all the children can be reaped
    ::prctl(PR_SET_CHILD_SUBREAPER, 1);
    const int pid = ::fork();
    if (pid == -1) {
      return absl::ErrnoToStatus(errno, "fork()");
    }
    switch (mode) {
      case Mode::kModeSandbox:
        handle_.emplace<internal::roma_service::ByobHandle>(
            pid, config.lib_mounts, progdir_, socket_name_, sockdir_,
            std::move(config.roma_container_name));
        break;
      case Mode::kModeNoSandbox:
        handle_.emplace<internal::roma_service::LocalHandle>(
            pid, config.lib_mounts, progdir_, socket_name_);
        break;
    }
    dispatcher_.emplace();
    PS_RETURN_IF_ERROR(dispatcher_->Init(fd));
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
    if (std::error_code ec; !std::filesystem::remove_all(progdir_, ec)) {
      LOG(ERROR) << "Failed to remove all " << progdir_ << ": " << ec;
    }
    if (::unlink(socket_name_) == -1) {
      PLOG(ERROR) << "Failed to unlink " << socket_name_;
    }
    delete socket_name_;
    if (std::error_code ec; !std::filesystem::remove(sockdir_, ec)) {
      LOG(ERROR) << "Failed to remove " << sockdir_ << ": " << ec;
    }
  }

  std::string LoadBinary(std::string_view code_path) {
    std::filesystem::path new_path =
        std::filesystem::path(progdir_) /
        ToString(::google::scp::core::common::Uuid::GenerateUuid());
    CHECK(std::filesystem::copy_file(code_path, new_path));
    return dispatcher_->LoadBinary(new_path.c_str(), n_workers_);
  }

  template <typename Message>
  absl::StatusOr<google::scp::roma::ExecutionToken> ExecuteBinary(
      std::string_view code_token, google::protobuf::Any request,
      TMetadata metadata,
      absl::AnyInvocable<void(absl::StatusOr<Message>) &&> callback) {
    dispatcher_->ExecuteBinary(
        code_token, std::move(request), std::move(metadata), function_bindings_,
        [callback = std::move(callback)](
            absl::StatusOr<std::string> response) mutable {
          if (response.ok()) {
            Message message;
            if (!message.ParseFromString(*response)) {
              std::move(callback)(absl::InternalError(
                  "Failed to deserialize response to proto"));
            }
            std::move(callback)(std::move(message));
          } else {
            std::move(callback)(std::move(response).status());
          }
        });
    return google::scp::roma::ExecutionToken(
        ToString(::google::scp::core::common::Uuid::GenerateUuid()));
  }

  template <typename Message>
  absl::StatusOr<google::scp::roma::ExecutionToken> ExecuteBinary(
      std::string_view code_token, google::protobuf::Any request,
      TMetadata metadata, absl::Notification& notif,
      absl::StatusOr<std::unique_ptr<Message>>& output) {
    dispatcher_->ExecuteBinary(
        code_token, std::move(request), std::move(metadata), function_bindings_,
        [&notif, &output](absl::StatusOr<std::string> response) {
          if (response.ok()) {
            // If response is uninitialized, initialize it with a unique_ptr.
            if (!output.ok() || *output == nullptr) {
              output = std::make_unique<Message>();
            }
            if (!(*output)->ParseFromString(*response)) {
              response = absl::InternalError(
                  "Failed to deserialize response to proto");
            }
          } else {
            output = std::move(response).status();
          }
          notif.Notify();
        });
    return google::scp::roma::ExecutionToken("dummy_id");
  }

 private:
  int n_workers_;
  char progdir_[20] = "/tmp/progdir_XXXXXX";
  char sockdir_[20] = "/tmp/sockdir_XXXXXX";
  const char* socket_name_ = nullptr;
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
