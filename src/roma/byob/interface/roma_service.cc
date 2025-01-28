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

#include "roma_service.h"

#include <sys/wait.h>
#include <unistd.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "src/roma/byob/config/config.h"
#include "src/roma/byob/utility/utils.h"

namespace privacy_sandbox::server_common::byob::internal::roma_service {

LocalHandle::LocalHandle(int pid, std::string_view mounts,
                         std::string_view control_socket_path,
                         std::string_view udf_socket_path,
                         std::string_view socket_dir, std::string_view log_dir,
                         bool enable_seccomp_filter)
    : pid_(pid) {
  // The following block does not run in the parent process.
  if (pid_ == 0) {
    PCHECK(::unshare(CLONE_NEWNS) == 0);
    // Set the process group id to the process id.
    PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
    const std::string root_dir =
        std::filesystem::path(CONTAINER_PATH) / CONTAINER_ROOT_RELPATH;
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
        sources_and_targets = {
            {log_dir, "/log_dir"},
            {socket_dir, "/socket_dir"},
            // Needs to be mounted for Cancel to work (kill by cmdline)
            {"/proc", "/proc"},
            {"/dev", "/dev"}};
    CHECK_OK(::privacy_sandbox::server_common::byob::SetupPivotRoot(
        root_dir, /*sources_and_targets_read_only=*/{},
        /*cleanup_pivot_root_dir=*/false, sources_and_targets,
        /*remount_root_as_read_only=*/false));
    const std::string mounts_flag = absl::StrCat("--mounts=", mounts);
    const std::string seccomp_filter_flag =
        absl::StrCat("--enable_seccomp_filter=", enable_seccomp_filter);
    const char* argv[] = {
        "/server/bin/run_workers",
        mounts_flag.c_str(),
        "--control_socket_name=/socket_dir/control.sock",
        "--udf_socket_name=/socket_dir/byob_rpc.sock",
        "--log_dir=/log_dir",
        seccomp_filter_flag.c_str(),
        nullptr,
    };
    const char* envp[] = {
        kLdLibraryPath.data(),
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]),
             const_cast<char* const*>(&envp[0]));
    PLOG(FATAL) << "execve()";
  }
}
LocalHandle::~LocalHandle() {
  ::kill(pid_, SIGTERM);
  // Wait for all processes in the process group to exit.
  uint32_t child_count = 0;
  while (::waitpid(-pid_, /*wstatus=*/nullptr, /*options=*/0) > 0) {
    child_count++;
  }
  if (child_count == 0) {
    PLOG(ERROR) << "waitpid unexpectedly didn't wait for any pids";
  }
}

NsJailHandle::NsJailHandle(int pid, std::string_view mounts,
                           std::string_view control_socket_path,
                           std::string_view udf_socket_path,
                           std::string_view socket_dir,
                           std::string container_name, std::string_view log_dir,
                           std::uint64_t memory_limit_soft,
                           std::uint64_t memory_limit_hard,
                           bool enable_seccomp_filter)
    : pid_(pid) {
  // The following block does not run in the parent process.
  if (pid_ == 0) {
    // Set the process group id to the process id.
    PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
    const std::string root_dir =
        std::filesystem::path(CONTAINER_PATH) / CONTAINER_ROOT_RELPATH;
    const std::string mounts_flag = absl::StrCat("--mounts=", mounts);
    const std::string seccomp_filter_flag =
        absl::StrCat("--enable_seccomp_filter=", enable_seccomp_filter);
    const std::string log_dir_mount = absl::StrCat(log_dir, ":/log_dir");
    const std::string socket_dir_mount =
        absl::StrCat(socket_dir, ":/socket_dir");
    const char* argv[] = {
        "/usr/byob/nsjail/bin/nsjail",
        "--mode",
        "o",  // MODE_STANDALONE_ONCE
        "--chroot",
        root_dir.data(),
        "--bindmount",
        log_dir_mount.c_str(),
        "--bindmount",
        socket_dir_mount.c_str(),
        "--bindmount_ro",
        "/dev/null",
        "--disable_rlimits",
        "--env",
        kLdLibraryPath.data(),
        "--forward_signals",
        "--keep_caps",
        "--quiet",
        "--rw",
        "--seccomp_string",
        kSeccompBpfPolicy.data(),
        "--",
        "/server/bin/run_workers",
        "--control_socket_name=/socket_dir/control.sock",
        "--log_dir=/log_dir",
        "--udf_socket_name=/socket_dir/byob_rpc.sock",
        mounts_flag.c_str(),
        seccomp_filter_flag.c_str(),
        nullptr,
    };
    const char* envp[] = {
        "LD_LIBRARY_PATH=/usr/byob/nsjail/lib",
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]),
             const_cast<char* const*>(&envp[0]));
    PLOG(FATAL) << "execve()";
  }
}
NsJailHandle::~NsJailHandle() {
  ::kill(pid_, SIGTERM);
  // Wait for all processes in the process group to exit.
  uint32_t child_count = 0;
  while (::waitpid(-pid_, /*wstatus=*/nullptr, /*options=*/0) > 0) {
    child_count++;
  }
  if (child_count == 0) {
    PLOG(ERROR) << "waitpid unexpectedly didn't wait for any pids";
  }
}

ByobHandle::ByobHandle(int pid, std::string_view mounts,
                       std::string_view control_socket_path,
                       std::string_view udf_socket_path,
                       std::string_view socket_dir, std::string container_name,
                       std::string_view log_dir,
                       std::uint64_t memory_limit_soft,
                       std::uint64_t memory_limit_hard, bool debug_mode,
                       bool enable_seccomp_filter)
    : pid_(pid),
      container_name_(container_name.empty() ? "default_roma_container_name"
                                             : std::move(container_name)) {
  // The following block does not run in the parent process.
  if (pid_ == 0) {
    // Set the process group id to the process id.
    PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
    std::filesystem::path container_config_path =
        std::filesystem::path(CONTAINER_PATH) / "config.json";
    CHECK(std::filesystem::exists(container_config_path));
    PCHECK(::close(STDIN_FILENO) == 0);
    nlohmann::json config;
    {
      std::ifstream ifs(container_config_path);
      config =
          nlohmann::json::parse(std::string(std::istreambuf_iterator<char>(ifs),
                                            std::istreambuf_iterator<char>()));
    }
    config["root"] = {{"path", CONTAINER_ROOT_RELPATH}};
    config["process"]["args"] = {
        "/server/bin/run_workers",
        absl::StrCat("--mounts=", mounts),
        "--control_socket_name=/socket_dir/control.sock",
        "--udf_socket_name=/socket_dir/byob_rpc.sock",
        "--log_dir=/log_dir",
        absl::StrCat("--enable_seccomp_filter=", enable_seccomp_filter),
    };
    config["process"]["rlimits"] = {};
    // If a memory limit has been configured, apply it.
    if (memory_limit_soft > 0 && memory_limit_hard > 0) {
      config["process"]["rlimits"] += {
          {"type", "RLIMIT_AS"},
          {"hard", memory_limit_hard},
          {"soft", memory_limit_soft},
      };
      // Having this config option does not help because of --ignore-cgroups
      // flag being used to initialize runsc
      config["linux"]["resources"]["memory"] = {
          {"limit", memory_limit_hard},
          {"reservation", memory_limit_hard},
          {"disableOOMKiller", true},
      };
    }
    config["mounts"] = {
        {
            {"source", socket_dir},
            {"destination", "/socket_dir"},
            {"type", "bind"},
            {"options", {"rbind", "rprivate"}},
        },
        {
            {"source", log_dir},
            {"destination", "/log_dir"},
            {"type", "bind"},
            {"options", {"rbind", "rprivate"}},
        },
    };
    if (!debug_mode) {
      config["process"]["rlimits"] += {
          {"type", "RLIMIT_CORE"},
          {"hard", 0},
          {"soft", 0},
      };
    }
    {
      std::ofstream ofs(container_config_path);
      ofs << config.dump();
    }
    PCHECK(::chdir(CONTAINER_PATH) == 0);
    // Note: Rootless runsc should be used judiciously. Since we have disabled
    // network stack (--network=none), rootless runsc should be side-effect
    // free.
    const char* debug_argv[] = {
        "/usr/byob/gvisor/bin/runsc",
        // runsc flags
        "--host-uds=all",
        "--ignore-cgroups",
        "--network=none",
        "--rootless",
        // debug flags
        "--debug",
        "--debug-log=/tmp/runsc-log/",
        "--strace",
        // command
        "run",
        container_name_.c_str(),
        nullptr,
    };
    const char* argv[] = {
        "/usr/byob/gvisor/bin/runsc",
        // runsc flags
        "--host-uds=all",
        "--ignore-cgroups",
        "--network=none",
        "--rootless",
        // command
        "run",
        container_name_.c_str(),
        nullptr,
    };
    ::execve(argv[0],
             const_cast<char* const*>(debug_mode ? &debug_argv[0] : &argv[0]),
             /*envp=*/nullptr);
    PLOG(FATAL) << "execve()";
  }
}

ByobHandle::~ByobHandle() {
  {
    const int pid = ::vfork();
    if (pid == 0) {
      const char* argv[] = {
          "/usr/byob/gvisor/bin/runsc",
          "kill",
          container_name_.c_str(),
          "SIGTERM",
          nullptr,
      };
      ::execve(argv[0], const_cast<char* const*>(&argv[0]), /*envp=*/nullptr);
      PLOG(FATAL) << "execve()";
    }
    ::waitpid(pid, nullptr, /*options=*/0);
  }
  // Wait for all processes in the process group to exit.
  uint32_t child_count = 0;
  while (::waitpid(-pid_, /*wstatus=*/nullptr, /*options=*/0) > 0) {
    child_count++;
  }
  if (child_count == 0) {
    PLOG(ERROR) << "waitpid unexpectedly didn't wait for any pids";
  }
  const char* argv[] = {
      "/usr/byob/gvisor/bin/runsc",
      // args
      "delete",
      "-force",
      container_name_.c_str(),
      // end args
      nullptr,
  };
  const int pid = ::vfork();
  if (pid == 0) {
    ::execve(argv[0], const_cast<char* const*>(&argv[0]),
             /*envp=*/nullptr);
    PLOG(FATAL) << "execve()";
  }
  ::waitpid(pid, nullptr, /*options=*/0);
}

}  // namespace privacy_sandbox::server_common::byob::internal::roma_service
