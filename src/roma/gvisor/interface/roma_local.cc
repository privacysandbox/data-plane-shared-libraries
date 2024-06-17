// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/roma/gvisor/interface/roma_local.h"

#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/proto/grpc/health/v1/health.grpc.pb.h"
#include "src/roma/gvisor/config/config.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::gvisor {

namespace {

absl::StatusOr<pid_t> RunLocalServer(const std::vector<const char*>& argv,
                                     std::shared_ptr<grpc::Channel> channel) {
  pid_t pid = vfork();
  if (pid == 0) {
    if (execv(argv[0], const_cast<char* const*>(argv.data())) == -1) {
      PLOG(ERROR) << absl::StrCat("Failed to execute '",
                                  absl::StrJoin(argv, " "), "'");
      exit(errno);
    }
  } else if (pid == -1) {
    return absl::ErrnoToStatus(errno,
                               absl::StrCat("Failed to fork before executing '",
                                            absl::StrJoin(argv, " "), "'"));
  }
  PS_RETURN_IF_ERROR(HealthCheckWithExponentialBackoff(std::move(channel)));
  return pid;
}
}  // namespace

RomaLocal::~RomaLocal() {
  if (kill(roma_server_pid_, SIGTERM) == -1) {
    PLOG(ERROR) << "Failed to kill Roma server process " << roma_server_pid_;
  }
  if (int status; waitpid(roma_server_pid_, &status, /*options=*/0) == -1) {
    PLOG(ERROR) << absl::StrCat("Failed to wait for ", roma_server_pid_);
  }
  if (std::error_code ec;
      std::filesystem::remove_all(socket_directory_, ec) <= 0) {
    LOG(ERROR) << "Failed to delete " << socket_directory_ << ": "
               << ec.message();
  }
  if (std::error_code ec;
      std::filesystem::remove_all(std::filesystem::path(prog_dir_), ec) <= 0) {
    LOG(ERROR) << "Failed to delete " << prog_dir_ << ": " << ec.message();
  }
}

absl::StatusOr<std::unique_ptr<RomaLocal>> RomaLocal::Create(
    Config config, ConfigInternal config_internal,
    std::shared_ptr<::grpc::Channel> channel) {
  PS_ASSIGN_OR_RETURN(std::string socket_pwd, CreateUniqueSocketName());
  PS_ASSIGN_OR_RETURN(std::string callback_socket, CreateUniqueSocketName());
  const std::filesystem::path server_path =
      config_internal.roma_container_dir /
      config_internal.roma_container_root_dir /
      config_internal.roma_server_path;
  std::string socket_path_flag =
      absl::StrCat("--", config_internal.socket_flag_name, "=",
                   config_internal.server_socket);
  std::string lib_mount_flag = absl::StrCat(
      "--", config_internal.lib_mounts_flag_name, "=", config.lib_mounts, ",",
      std::filesystem::path(config_internal.callback_socket)
          .parent_path()
          .c_str());
  std::string num_workers_flag =
      absl::StrCat("--", config_internal.worker_pool_size_flag_name, "=",
                   config.num_workers);
  const std::string callback_socket_flag =
      absl::StrCat("--", config_internal.callback_socket_flag_name, "=",
                   config_internal.callback_socket);
  const std::string prog_dir_flag = absl::StrCat(
      "--", config_internal.prog_dir_flag_name, "=", config_internal.prog_dir);
  std::vector<const char*> run_local_server = {
      server_path.c_str(),
      socket_path_flag.c_str(),
      lib_mount_flag.c_str(),
      num_workers_flag.c_str(),
      callback_socket_flag.c_str(),
      prog_dir_flag.c_str(),
      nullptr,
  };
  PS_ASSIGN_OR_RETURN(pid_t pid,
                      RunLocalServer(run_local_server, std::move(channel)));
  // Note that since RomaLocal's constructor is private, we have to use new.
  return absl::WrapUnique(new RomaLocal(
      std::move(config), pid,
      std::filesystem::path(config_internal.server_socket).parent_path(),
      std::filesystem::path(config_internal.prog_dir)));
}
}  // namespace privacy_sandbox::server_common::gvisor
