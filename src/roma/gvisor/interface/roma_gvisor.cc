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

#include "src/roma/gvisor/interface/roma_gvisor.h"

#include <sys/prctl.h>
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

#include <nlohmann/json.hpp>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "src/roma/gvisor/config/config.h"
#include "src/roma/gvisor/interface/roma_api.pb.h"
#include "src/util/status_macro/status_macros.h"

namespace privacy_sandbox::server_common::gvisor {

namespace {

const std::filesystem::path kConfigJson = "config.json";
// TODO: ashruti - Move this to config

// Performs pre-checks on the Roma gVisor OCI container directory. Namely,
// it checks that the image directory -
// - contains config.json file
// - contains a root folder
absl::Status PreCheckRomaContainerDir(const ConfigInternal* config) {
  if (!std::filesystem::exists(config->roma_container_dir)) {
    return absl::InternalError(absl::StrCat("Roma container directory ",
                                            config->roma_container_dir.c_str(),
                                            " does not exit"));
  }
  if (!std::filesystem::exists(config->roma_container_dir / kConfigJson)) {
    return absl::InternalError(absl::StrCat(
        "Roma container directory does not contain ", kConfigJson.c_str()));
  }
  if (!std::filesystem::exists(config->roma_container_dir /
                               config->roma_container_root_dir)) {
    return absl::InternalError(
        absl::StrCat("Roma container directory does not contain ",
                     config->roma_container_root_dir.c_str()));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::string> ReadFromFile(
    const std::filesystem::path& file_path) {
  std::ifstream file_stream(file_path);
  if (!file_stream.is_open() || file_stream.fail()) {
    return absl::InternalError(
        absl::StrCat("Failed to open ", file_path.c_str(), " for reading"));
  }
  std::stringstream file_buf;
  file_buf << file_stream.rdbuf();
  file_stream.close();
  return file_buf.str();
}

absl::Status WriteToFile(const std::filesystem::path& file_path,
                         std::string_view content) {
  std::ofstream file_stream(file_path);
  if (!file_stream.is_open()) {
    return absl::InternalError(
        absl::StrCat("Failed to open ", file_path.c_str(), " for writing."));
  }
  file_stream << content;
  file_stream.close();
  return absl::OkStatus();
}

absl::Status ModifyContainerConfigJson(
    const Config* roma_config, const ConfigInternal* roma_config_internal,
    const std::filesystem::path& socket_pwd,
    std::filesystem::path callback_socket) {
  const std::filesystem::path config_file_loc =
      roma_config_internal->roma_container_dir / kConfigJson;
  // Read the existing config.json file.
  PS_ASSIGN_OR_RETURN(std::string config_json_contents,
                      ReadFromFile(config_file_loc));
  nlohmann::json config_nlohmann_json =
      nlohmann::json::parse(config_json_contents);
  // Add root path to config.
  config_nlohmann_json["root"] = {
      {"path", roma_config_internal->roma_container_root_dir.c_str()}};
  std::filesystem::path socket_dir = socket_pwd.parent_path();
  // Bind-mount socket directory for unix-domain sockets. The sockets from this
  // directory will be used for communication between gVisor and outside world.
  nlohmann::json sock_dir_mount = {
      {"destination", socket_dir.c_str()},
      {"type", "bind"},
      {"source", socket_dir.c_str()},
      {"options", nlohmann::json{"rbind", "rprivate"}},
  };
  std::filesystem::path callback_socket_dir = callback_socket.parent_path();
  nlohmann::json callback_sock_dir_mount = {
      {"destination", callback_socket_dir.c_str()},
      {"type", "bind"},
      {"source", callback_socket_dir.c_str()},
      {"options", nlohmann::json{"rbind", "rprivate"}},
  };
  nlohmann::json prog_dir_mount = {
      {"destination", roma_config_internal->prog_dir.c_str()},
      {"type", "volume"},
      {"source", roma_config_internal->prog_dir.c_str()},
      {"options", nlohmann::json{"rbind", "rprivate"}},
  };
  config_nlohmann_json["mounts"] = nlohmann::json::array(
      {sock_dir_mount, callback_sock_dir_mount, prog_dir_mount});
  // Add args to startup gVisor server.
  config_nlohmann_json["process"]["args"] = {
      roma_config_internal->roma_server_path.c_str(),
      // Share socket path with the server.
      absl::StrCat("--", roma_config_internal->socket_flag_name, "=",
                   socket_pwd.c_str()),
      absl::StrCat("--", roma_config_internal->lib_mounts_flag_name, "=",
                   roma_config->lib_mounts, ",",
                   callback_socket.parent_path().c_str()),
      absl::StrCat("--", roma_config_internal->worker_pool_size_flag_name, "=",
                   roma_config->num_workers),
      absl::StrCat("--", roma_config_internal->callback_socket_flag_name, "=",
                   callback_socket.c_str()),
      absl::StrCat("--", roma_config_internal->prog_dir_flag_name, "=",
                   roma_config_internal->prog_dir)};
  return WriteToFile(config_file_loc, config_nlohmann_json.dump());
}

absl::StatusOr<pid_t> RunGvisorContainer(
    const Config* config, const ConfigInternal* config_internal,
    const std::filesystem::path& socket_pwd,
    std::shared_ptr<grpc::Channel> gvisor_channel) {
  std::vector<const char*> argv_runsc = {
      config_internal->runsc_path.c_str(), "--host-uds=all", "run",
      config->roma_container_name.c_str(), nullptr};
  // Need to set this to ensure all the children can be reaped
  prctl(PR_SET_CHILD_SUBREAPER, 1);
  pid_t pid = vfork();
  if (pid == 0) {
    if (chdir(config_internal->roma_container_dir.c_str()) == -1) {
      PLOG(ERROR) << "Failed to chdir to "
                  << config_internal->roma_container_dir;
      exit(errno);
    }
    if (execv(config_internal->runsc_path.c_str(),
              const_cast<char* const*>(argv_runsc.data())) == -1) {
      PLOG(ERROR) << "Failed to execv Roma gVisor container '"
                  << absl::StrJoin(argv_runsc, " ") << "'";
      exit(errno);
    }
  } else if (pid == -1) {
    return absl::ErrnoToStatus(errno, "Failed to vfork.");
  }
  PS_RETURN_IF_ERROR(
      HealthCheckWithExponentialBackoff(std::move(gvisor_channel)));
  return pid;
}

void RunCommand(const std::vector<const char*>& argv) {
  pid_t pid = vfork();
  if (pid == 0) {
    if (execv(argv[0], const_cast<char* const*>(argv.data())) == -1) {
      PLOG(ERROR) << absl::StrCat("Failed to execute '",
                                  absl::StrJoin(argv, " "), "'");
      exit(errno);
    }
  } else if (pid == -1) {
    PLOG(ERROR) << absl::StrCat("Failed to fork before executing '",
                                absl::StrJoin(argv, " "), "'");
    return;
  }
  if (int status; waitpid(pid, &status, /*options=*/0) == -1) {
    PLOG(ERROR) << absl::StrCat("Failed to wait for '",
                                absl::StrJoin(argv, " "), "'");
    return;
  } else if (!WIFEXITED(status)) {
    LOG(ERROR) << absl::StrCat(
        "Process exited abnormally while executing command '",
        absl::StrJoin(argv, " "), "'");
    return;
  } else if (const int child_errno = WEXITSTATUS(status);
             child_errno != EXIT_SUCCESS) {
    LOG(ERROR) << absl::StrCat(
        "Child process exited with non-zero error number ", child_errno,
        " while executing command '", absl::StrJoin(argv, " "),
        "': ", strerror(child_errno));
    return;
  }
}
}  // namespace

RomaGvisor::~RomaGvisor() {
  // Observed 'gofer is still running' issue -
  // https://github.com/google/gvisor/issues/6255
  std::vector<const char*> runsc_kill = {
      config_internal_.runsc_path.c_str(),
      "kill",
      config_.roma_container_name.c_str(),
      "SIGTERM",
      nullptr,
  };
  RunCommand(runsc_kill);
  if (kill(roma_container_pid_, SIGTERM) == -1) {
    PLOG(ERROR) << "Failed to kill Roma server process " << roma_container_pid_;
  }
  if (int status; waitpid(roma_container_pid_, &status, /*options=*/0) == -1) {
    PLOG(ERROR) << absl::StrCat("Failed to wait for ", roma_container_pid_);
  }
  std::vector<const char*> runsc_delete = {
      config_internal_.runsc_path.c_str(), "delete", "-force",
      config_.roma_container_name.c_str(), nullptr,
  };
  RunCommand(runsc_delete);
  std::filesystem::path server_socket_dir =
      std::filesystem::path(config_internal_.server_socket).parent_path();
  if (std::error_code ec;
      std::filesystem::remove_all(server_socket_dir, ec) <= 0) {
    LOG(ERROR) << "Failed to delete " << server_socket_dir << ": "
               << ec.message();
  }
  if (std::error_code ec;
      std::filesystem::remove_all(
          std::filesystem::path(config_internal_.prog_dir), ec) <= 0) {
    LOG(ERROR) << "Failed to delete " << config_internal_.prog_dir << ": "
               << ec.message();
  }
}

absl::StatusOr<std::unique_ptr<RomaGvisor>> RomaGvisor::Create(
    Config config, ConfigInternal config_internal,
    std::shared_ptr<::grpc::Channel> channel) {
  PS_RETURN_IF_ERROR(PreCheckRomaContainerDir(&config_internal));
  PS_ASSIGN_OR_RETURN(const std::filesystem::path socket_pwd,
                      CreateUniqueSocketName());
  PS_ASSIGN_OR_RETURN(std::string callback_socket, CreateUniqueSocketName());
  PS_RETURN_IF_ERROR(ModifyContainerConfigJson(
      &config, &config_internal, config_internal.server_socket,
      config_internal.callback_socket));
  PS_ASSIGN_OR_RETURN(
      pid_t pid,
      RunGvisorContainer(&config, &config_internal,
                         config_internal.server_socket, std::move(channel)));
  // Note that since RomaGvisor's constructor is private, we have to use new.
  return absl::WrapUnique(new RomaGvisor(
      std::move(config), std::move(config_internal), pid,
      std::filesystem::path(config_internal.server_socket).parent_path()));
}
}  // namespace privacy_sandbox::server_common::gvisor
