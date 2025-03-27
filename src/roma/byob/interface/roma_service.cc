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

#include <sys/mount.h>
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
namespace {
int LocalImpl(void* arg) {
  const auto& options = *static_cast<Handle::Options*>(arg);
  // Set the process group id to the process id.
  PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
  const std::string root_dir =
      std::filesystem::path(CONTAINER_PATH) / CONTAINER_ROOT_RELPATH;
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets = {
          {options.log_dir, "/log_dir"},
          {options.socket_dir, "/socket_dir"},
          {options.binary_dir, "/binary_dir"},
          // Needs to be mounted for Cancel to work (kill by cmdline)
          {"/proc", "/proc"},
          {"/dev", "/dev"}};

  // Remount /proc to ensure it reflects namespace changes.
  PCHECK(::mount("/proc", "/proc", "proc", /*mountflags=*/0, nullptr) == 0);
  CHECK_OK(::privacy_sandbox::server_common::byob::SetupPivotRoot(
      root_dir, /*sources_and_targets_read_only=*/{},
      /*cleanup_pivot_root_dir=*/false, sources_and_targets,
      /*remount_root_as_read_only=*/false));
  const std::string mounts_flag = absl::StrCat("--mounts=", options.mounts);
  const std::string syscall_filtering_flag = absl::StrCat(
      "--syscall_filtering=", AbslUnparseFlag(options.syscall_filtering));
  const std::string ipc_namespace_flag =
      absl::StrCat("--disable_ipc_namespace=", options.disable_ipc_namespace);
  const char* argv[] = {
      "/server/bin/run_workers",
      mounts_flag.c_str(),
      "--control_socket_name=/socket_dir/control.sock",
      "--udf_socket_name=/socket_dir/byob_rpc.sock",
      "--log_dir=/log_dir",
      "--binary_dir=/binary_dir",
      syscall_filtering_flag.c_str(),
      ipc_namespace_flag.c_str(),
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
class LocalHandle final : public Handle {
 public:
  LocalHandle(int pid) : pid_(pid) {}
  LocalHandle(const LocalHandle&) = delete;
  LocalHandle(LocalHandle&&) = delete;
  LocalHandle& operator=(const LocalHandle&) = delete;
  LocalHandle& operator=(LocalHandle&&) = delete;
  ~LocalHandle() override {
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

 private:
  int pid_;
};

int NsjailImpl(void* arg) {
  const auto& options = *static_cast<Handle::Options*>(arg);
  // Set the process group id to the process id.
  PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
  const std::string root_dir =
      std::filesystem::path(CONTAINER_PATH) / CONTAINER_ROOT_RELPATH;
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets = {
          {options.log_dir, "/log_dir"},
          {options.socket_dir, "/socket_dir"},
          // Needs to be mounted for Cancel to work (kill by cmdline)
          {"/proc", "/proc"},
          {"/dev", "/dev"},
          {options.binary_dir, "/binary_dir"},
      };
  CHECK_OK(::privacy_sandbox::server_common::byob::SetupPivotRoot(
      root_dir, /*sources_and_targets_read_only=*/{},
      /*cleanup_pivot_root_dir=*/false, sources_and_targets,
      /*remount_root_as_read_only=*/false));
  const std::string mounts_flag = absl::StrCat("--mounts=", options.mounts);
  const std::string syscall_filtering_flag = absl::StrCat(
      "--syscall_filtering=", AbslUnparseFlag(options.syscall_filtering));
  const std::string ipc_namespace_flag =
      absl::StrCat("--disable_ipc_namespace=", options.disable_ipc_namespace);
  const char* argv[] = {
      "/usr/bin/nsjail",
      "--mode",
      "o",  // MODE_STANDALONE_ONCE
      "--chroot",
      "/",
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
      "--binary_dir=/binary_dir",
      mounts_flag.c_str(),
      syscall_filtering_flag.c_str(),
      ipc_namespace_flag.c_str(),
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

int GvisorImpl(void* arg) {
  const auto& options = *static_cast<Handle::Options*>(arg);
  // Set the process group id to the process id.
  PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
  std::filesystem::path container_config_path =
      std::filesystem::path(CONTAINER_PATH) / "config.json";
  CHECK(std::filesystem::exists(container_config_path));
  PCHECK(::close(STDIN_FILENO) == 0);
  nlohmann::json config;
  {
    std::ifstream ifs(container_config_path);
    config = nlohmann::json::parse(std::string(
        std::istreambuf_iterator<char>(ifs), std::istreambuf_iterator<char>()));
  }
  config["root"] = {{"path", CONTAINER_ROOT_RELPATH}};
  config["process"]["args"] = {
      "/server/bin/run_workers",
      absl::StrCat("--mounts=", options.mounts),
      "--control_socket_name=/socket_dir/control.sock",
      "--udf_socket_name=/socket_dir/byob_rpc.sock",
      "--log_dir=/log_dir",
      "--binary_dir=/binary_dir",
      absl::StrCat("--syscall_filtering=",
                   AbslUnparseFlag(options.syscall_filtering)),
      absl::StrCat("--disable_ipc_namespace=", options.disable_ipc_namespace),
  };
  config["process"]["rlimits"] = {};
  // If a memory limit has been configured, apply it.
  if (options.memory_limit_soft > 0 && options.memory_limit_hard > 0) {
    config["process"]["rlimits"] += {
        {"type", "RLIMIT_AS"},
        {"hard", options.memory_limit_hard},
        {"soft", options.memory_limit_soft},
    };
    // Having this config option does not help because of --ignore-cgroups
    // flag being used to initialize runsc
    config["linux"]["resources"]["memory"] = {
        {"limit", options.memory_limit_hard},
        {"reservation", options.memory_limit_hard},
        {"disableOOMKiller", true},
    };
  }
  config["mounts"] = {
      {
          {"source", options.socket_dir},
          {"destination", "/socket_dir"},
          {"type", "bind"},
          {"options", {"rbind", "rprivate"}},
      },
      {
          {"source", options.log_dir},
          {"destination", "/log_dir"},
          {"type", "bind"},
          {"options", {"rbind", "rprivate"}},
      },
      {
          {"source", options.binary_dir},
          {"destination", "/binary_dir"},
          {"type", "bind"},
          {"options", {"rbind", "rprivate"}},
      },
  };
  if (!options.debug_mode) {
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
      options.container_name.c_str(),
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
      options.container_name.c_str(),
      nullptr,
  };
  ::execve(
      argv[0],
      const_cast<char* const*>(options.debug_mode ? &debug_argv[0] : &argv[0]),
      /*envp=*/nullptr);
  PLOG(FATAL) << "execve()";
}
class GvisorHandle final : public Handle {
 public:
  GvisorHandle(int pid, std::string container_name)
      : pid_(pid), container_name_(std::move(container_name)) {}
  GvisorHandle(const GvisorHandle&) = delete;
  GvisorHandle(GvisorHandle&&) = delete;
  GvisorHandle& operator=(const GvisorHandle&) = delete;
  GvisorHandle& operator=(GvisorHandle&&) = delete;
  ~GvisorHandle() override {
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
      ::execve(argv[0], const_cast<char* const*>(&argv[0]), /*envp=*/nullptr);
      PLOG(FATAL) << "execve()";
    }
    ::waitpid(pid, nullptr, /*options=*/0);
  }

 private:
  int pid_;
  std::string container_name_;
};
}  // namespace
absl::StatusOr<std::unique_ptr<Handle>> Handle::CreateHandle(
    Mode mode, Handle::Options options) {
  alignas(16) char stack[1 << 20];
  switch (mode) {
    case Mode::kModeGvisorSandbox:
      [[fallthrough]];
    case Mode::kModeGvisorSandboxDebug: {
      const int pid = ::clone(&GvisorImpl, stack + sizeof(stack),
                              CLONE_VM | CLONE_VFORK, &options);
      if (pid == -1) {
        return absl::ErrnoToStatus(errno, "clone()");
      }
      return std::make_unique<GvisorHandle>(pid,
                                            std::move(options.container_name));
    }
    case Mode::kModeMinimalSandbox: {
      // CLONE_NEWPID is needed to ensure run_workers can properly reap the
      // processes it creates.
      const int pid = ::clone(
          &LocalImpl, stack + sizeof(stack),
          CLONE_VM | CLONE_VFORK | CLONE_NEWNS | CLONE_NEWPID, &options);
      if (pid == -1) {
        return absl::ErrnoToStatus(errno, "clone()");
      }
      return std::make_unique<LocalHandle>(pid);
    }
    case Mode::kModeNsJailSandbox: {
      const int pid = ::clone(&NsjailImpl, stack + sizeof(stack),
                              CLONE_VM | CLONE_VFORK | CLONE_NEWNS, &options);
      if (pid == -1) {
        return absl::ErrnoToStatus(errno, "clone()");
      }
      return std::make_unique<LocalHandle>(pid);
    }
    default:
      return absl::InternalError("Unsupported mode in switch");
  }
}

}  // namespace privacy_sandbox::server_common::byob::internal::roma_service
