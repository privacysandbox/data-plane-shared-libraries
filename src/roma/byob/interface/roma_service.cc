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

#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

#include <nlohmann/json.hpp>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"

namespace privacy_sandbox::server_common::byob::internal::roma_service {
LocalHandle::LocalHandle(int pid, std::string_view mounts,
                         std::string_view progdir, std::string_view socket_name)
    : pid_(pid) {
  // The following block does not run in the parent process.
  if (pid_ == 0) {
    const std::string run_workers_path = std::filesystem::path(CONTAINER_PATH) /
                                         CONTAINER_ROOT_RELPATH /
                                         "server/bin/run_workers";
    const std::string mounts_flag =
        absl::StrCat("--mounts=", mounts.empty() ? LIB_MOUNTS : mounts);
    const std::string progdir_flag = absl::StrCat("--progdir=", progdir);
    const std::string socket_name_flag =
        absl::StrCat("--socket_name=", socket_name);
    const char* argv[] = {
        run_workers_path.c_str(),
        mounts_flag.c_str(),
        progdir_flag.c_str(),
        socket_name_flag.c_str(),
        nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve()";
  }
}
LocalHandle::~LocalHandle() {
  if (::waitpid(pid_, nullptr, /*options=*/0) == -1) {
    PLOG(ERROR) << "waitpid(" << pid_ << ", nullptr, 0)";
  }
}

ByobHandle::ByobHandle(int pid, std::string_view mounts,
                       std::string_view progdir, std::string_view socket_name,
                       std::string_view sockdir, std::string container_name)
    : pid_(pid),
      container_name_(container_name.empty() ? "default_roma_container_name"
                                             : std::move(container_name)) {
  // The following block does not run in the parent process.
  if (pid_ == 0) {
    nlohmann::json config;
    {
      std::ifstream ifs(std::filesystem::path(CONTAINER_PATH) / "config.json");
      config =
          nlohmann::json::parse(std::string(std::istreambuf_iterator<char>(ifs),
                                            std::istreambuf_iterator<char>()));
    }
    config["root"] = {{"path", CONTAINER_ROOT_RELPATH}};
    config["process"]["args"] = {
        "server/bin/run_workers",
        absl::StrCat("--mounts=", mounts.empty() ? LIB_MOUNTS : mounts),
        absl::StrCat("--progdir=", progdir),
        absl::StrCat("--socket_name=", socket_name),
    };
    config["mounts"] = {
        {
            {"destination", progdir},
            {"type", "bind"},
            {"source", progdir},
            {"options", {"rbind", "rprivate"}},
        },
        {
            {"destination", sockdir},
            {"type", "bind"},
            {"source", sockdir},
            {"options", {"rbind", "rprivate"}},
        },
    };
    {
      std::ofstream ofs(std::filesystem::path(CONTAINER_PATH) / "config.json");
      ofs << config.dump();
    }
    PCHECK(::chdir(CONTAINER_PATH) == 0);
    const char* argv[] = {
        "/usr/bin/runsc",        "--host-uds=all", "--ignore-cgroups", "run",
        container_name_.c_str(), nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve()";
  }
}
ByobHandle::~ByobHandle() {
  if (::waitpid(pid_, nullptr, /*options=*/0) == -1) {
    PLOG(ERROR) << "waitpid(" << pid_ << ", nullptr, 0)";
  }
  const int pid = ::vfork();
  if (pid == 0) {
    const char* argv[] = {
        "/usr/bin/runsc", "delete", "-force", container_name_.c_str(), nullptr,
    };
    ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
    PLOG(FATAL) << "execve()";
  }
  ::waitpid(pid, nullptr, /*options=*/0);
}
}  // namespace privacy_sandbox::server_common::byob::internal::roma_service
