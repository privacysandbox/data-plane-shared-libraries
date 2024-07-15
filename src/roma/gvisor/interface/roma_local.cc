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
  std::error_code ec;
  if (std::filesystem::path socket_directory =
          std::filesystem::path(config_.server_socket).parent_path();
      std::filesystem::remove_all(socket_directory, ec) <= 0) {
    LOG(ERROR) << "Failed to delete " << socket_directory.c_str() << ": "
               << ec.message();
  }
  if (std::filesystem::remove_all(std::filesystem::path(config_.prog_dir),
                                  ec) <= 0) {
    LOG(ERROR) << "Failed to delete " << config_.prog_dir << ": "
               << ec.message();
  }
}

absl::StatusOr<std::unique_ptr<RomaLocal>> RomaLocal::Create(
    ConfigInternal config, std::shared_ptr<::grpc::Channel> channel) {
  const std::filesystem::path server_path = config.roma_container_dir /
                                            config.roma_container_root_dir /
                                            config.roma_server_path;
  std::string socket_path_flag =
      absl::StrCat("--", config.socket_flag_name, "=", config.server_socket);
  std::string lib_mount_flag = absl::StrCat(
      "--", config.lib_mounts_flag_name, "=", config.lib_mounts, ",",
      std::filesystem::path(config.callback_socket).parent_path().c_str());
  std::string num_workers_flag = absl::StrCat(
      "--", config.worker_pool_size_flag_name, "=", config.num_workers);
  const std::string callback_socket_flag = absl::StrCat(
      "--", config.callback_socket_flag_name, "=", config.callback_socket);
  const std::string prog_dir_flag =
      absl::StrCat("--", config.prog_dir_flag_name, "=", config.prog_dir);
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
  return absl::WrapUnique(new RomaLocal(std::move(config), pid));
}
}  // namespace privacy_sandbox::server_common::gvisor
