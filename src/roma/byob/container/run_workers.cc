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

#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <filesystem>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <grpcpp/security/server_credentials.h>
#include <grpcpp/server.h>
#include <grpcpp/server_builder.h>
#include <grpcpp/server_context.h>

#include <uuid/uuid.h>

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/base/thread_annotations.h"
#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/core/common/uuid/uuid.h"
#include "src/roma/byob/dispatcher/dispatcher.grpc.pb.h"
#include "src/roma/byob/dispatcher/dispatcher.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/roma/byob/utility/utils.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

ABSL_FLAG(std::string, control_socket_name, "/sockdir/xyzw.sock",
          "Socket for host to run_workers communication");
ABSL_FLAG(std::string, udf_socket_name, "/sockdir/abcd.sock",
          "Socket for host to UDF communication");
ABSL_FLAG(std::vector<std::string>, mounts,
          std::vector<std::string>({"/lib", "/lib64"}),
          "Mounts containing dependencies needed by the binary");
ABSL_FLAG(std::string, log_dir, "/log_dir", "Directory used for storing logs");

namespace {
using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::privacy_sandbox::server_common::byob::CancelRequest;
using ::privacy_sandbox::server_common::byob::CancelResponse;
using ::privacy_sandbox::server_common::byob::DeleteBinaryRequest;
using ::privacy_sandbox::server_common::byob::DeleteBinaryResponse;
using ::privacy_sandbox::server_common::byob::kNumTokenBytes;
using ::privacy_sandbox::server_common::byob::LoadBinaryRequest;
using ::privacy_sandbox::server_common::byob::LoadBinaryResponse;
using ::privacy_sandbox::server_common::byob::WorkerRunnerService;

const absl::NoDestructor<std::filesystem::path> kBinaryExe("bin.exe");

std::string GenerateUuid() {
  uuid_t uuid;
  uuid_generate(uuid);
  std::string uuid_cstr(kNumTokenBytes, '\0');
  uuid_unparse(uuid, uuid_cstr.data());
  return uuid_cstr;
}

absl::Status ConnectToPath(const int fd, std::string_view socket_name) {
  ::sockaddr_un sa = {
      .sun_family = AF_UNIX,
  };
  socket_name.copy(sa.sun_path, sizeof(sa.sun_path));
  if (::connect(fd, reinterpret_cast<::sockaddr*>(&sa), SUN_LEN(&sa)) == -1) {
    return absl::ErrnoToStatus(errno, "connect()");
  }
  return absl::OkStatus();
}

// WorkerImplArg contains references to objects whose lifetimes are managed
// elsewhere. This is considered problematic. However, it is safe in this case
// because worker clones are created with CLONE_VFORK and CLONE_VM flag. These
// flags ensure that the calling process does not modify/delete the content
// before the worker calls exec.
struct WorkerImplArg {
  const std::filesystem::path& pivot_root_dir;
  absl::Span<const std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets;
  std::string_view execution_token;
  std::string_view socket_name;
  std::string_view code_token;
  const std::filesystem::path& binary_path;
  int dev_null_fd;
  bool enable_log_egress;
  std::string_view log_dir_name;
};

absl::Status SetPrctlOptions(
    absl::Span<const std::pair<int, int>> option_arg_pairs) {
  for (const auto& [option, arg] : option_arg_pairs) {
    if (::prctl(option, arg) < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed prctl(", option, ", ", arg, ")"));
    }
  }
  return absl::OkStatus();
}

absl::StatusOr<std::filesystem::path> CreatePivotRootDir() {
  std::string pivot_root_dir = "/tmp/pivot_root_XXXXXX";
  if (::mkdtemp(pivot_root_dir.data()) == nullptr) {
    return absl::ErrnoToStatus(errno, "mkdtemp()");
  }
  return pivot_root_dir;
}

absl::Status Dup2(int oldfd, int newfd) {
  if (::dup2(oldfd, newfd) == -1) {
    return absl::ErrnoToStatus(errno,
                               absl::StrCat("dup2(", oldfd, ",", newfd, ")"));
  }
  return absl::OkStatus();
}

absl::Status SetupSandbox(const WorkerImplArg& worker_impl_arg) {
  int log_fd = -1;
  if (worker_impl_arg.enable_log_egress) {
    const std::filesystem::path log_file_name =
        std::filesystem::path(worker_impl_arg.log_dir_name) /
        absl::StrCat(worker_impl_arg.execution_token, ".log");
    log_fd = ::open(log_file_name.c_str(), O_CREAT | O_WRONLY | O_APPEND, 0644);
    if (log_fd < 0) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to open '", log_file_name.native(), "'"));
    }
  }
  PS_RETURN_IF_ERROR(SetPrctlOptions({
      {PR_CAPBSET_DROP, CAP_SYS_ADMIN},
      {PR_CAPBSET_DROP, CAP_SETPCAP},
      {PR_SET_PDEATHSIG, SIGHUP},
  }));
  if (worker_impl_arg.enable_log_egress) {
    PS_RETURN_IF_ERROR(Dup2(log_fd, STDOUT_FILENO));
    PS_RETURN_IF_ERROR(Dup2(log_fd, STDERR_FILENO));
  }
  return ::privacy_sandbox::server_common::byob::SetupPivotRoot(
      worker_impl_arg.pivot_root_dir, worker_impl_arg.sources_and_targets);
}

constexpr uint32_t MaxIntDecimalLength() {
  int n = std::numeric_limits<int>::max();
  uint32_t count = 0;
  while (n > 0) {
    n /= 10;
    count++;
  }
  return count;
}

int WorkerImpl(void* arg) {
  const WorkerImplArg& worker_impl_arg = *static_cast<WorkerImplArg*>(arg);
  const int rpc_fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
  PCHECK(rpc_fd != -1);
  if (const absl::Status status =
          ConnectToPath(rpc_fd, worker_impl_arg.socket_name);
      !status.ok()) {
    LOG(INFO) << status;
    return -1;
  }
  PCHECK(::write(rpc_fd, worker_impl_arg.code_token.data(), kNumTokenBytes) ==
         kNumTokenBytes);
  PCHECK(::write(rpc_fd, worker_impl_arg.execution_token.data(),
                 kNumTokenBytes) == kNumTokenBytes);

  // Add one to decimal length because `snprintf` adds a null terminator.
  char connection_fd[MaxIntDecimalLength() + 1];
  PCHECK(::snprintf(connection_fd, sizeof(connection_fd), "%d", rpc_fd) > 0);

  // Destructors will not run after `exec`. All objects must be destroyed and
  // all heap allocations must be freed prior to `exec`.
  CHECK_OK(SetupSandbox(worker_impl_arg));

  // Exec binary.
  // Setting cmdline (argv[0]) to a value distinct from the binary path is
  // unconventional but allows us to lookup the process by execution_token
  // in `KillByCmdLine`.
  ::execl(worker_impl_arg.binary_path.c_str(),
          worker_impl_arg.execution_token.data(), connection_fd, nullptr);
  PLOG(FATAL) << "exec '" << worker_impl_arg.binary_path << "' failed";
}
struct ReloaderImplArg {
  std::vector<std::string> mounts;
  std::string socket_name;
  std::string code_token;
  std::filesystem::path binary_path;
  int dev_null_fd;
  bool enable_log_egress;
  std::string log_dir_name;
};

std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
GetSourcesAndTargets(const std::filesystem::path& pivot_root_dir,
                     absl::Span<const std::string> mounts) {
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets;
  sources_and_targets.reserve(mounts.size());
  for (const std::filesystem::path mount : mounts) {
    sources_and_targets.push_back(
        {mount, pivot_root_dir / mount.relative_path()});
  }
  return sources_and_targets;
}

int ReloaderImpl(void* arg) {
  // Group workers with reloaders so they can be killed together.
  PCHECK(::setpgid(/*pid=*/0, /*pgid=*/0) == 0);
  const ReloaderImplArg& reloader_impl_arg =
      *static_cast<ReloaderImplArg*>(arg);
  CHECK_OK(Dup2(reloader_impl_arg.dev_null_fd, STDOUT_FILENO));
  CHECK_OK(Dup2(reloader_impl_arg.dev_null_fd, STDERR_FILENO));
  const absl::StatusOr<std::filesystem::path> pivot_root_dir =
      CreatePivotRootDir();
  CHECK_OK(pivot_root_dir);
  const std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets =
          GetSourcesAndTargets(*pivot_root_dir, reloader_impl_arg.mounts);
  {
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
        reloader_sources_and_targets = sources_and_targets;
    const std::filesystem::path socket_dir =
        std::filesystem::path(reloader_impl_arg.socket_name).parent_path();

    // SetupPivotRoot reduces the base filesystem image size. This pivot root
    // includes the socket_dir, which must not be shared with the pivot_root
    // created by the worker.
    reloader_sources_and_targets.push_back(
        {socket_dir, *pivot_root_dir / socket_dir.relative_path()});
    CHECK_OK(::privacy_sandbox::server_common::byob::SetupPivotRoot(
        *pivot_root_dir, reloader_sources_and_targets));
  }
  while (true) {
    // Start a new worker.
    const std::string execution_token = GenerateUuid();
    WorkerImplArg worker_impl_arg{
        .pivot_root_dir = *pivot_root_dir,
        .sources_and_targets = sources_and_targets,
        .execution_token = execution_token,
        .socket_name = reloader_impl_arg.socket_name,
        .code_token = reloader_impl_arg.code_token,
        .binary_path = reloader_impl_arg.binary_path,
        .dev_null_fd = reloader_impl_arg.dev_null_fd,
        .enable_log_egress = reloader_impl_arg.enable_log_egress,
        .log_dir_name = reloader_impl_arg.log_dir_name,
    };

    // Explicitly 16-byte align the stack. Otherwise, `clone` on aarch64 may
    // hang or the process may receive SIGBUS (depending on the size of the
    // stack before this function call). Overprovisions stack by at most 15
    // bytes (of 2^10 bytes) where unneeded.
    // https://community.arm.com/arm-community-blogs/b/architectures-and-processors-blog/posts/using-the-stack-in-aarch32-and-aarch64
    alignas(16) char stack[1 << 20];
    const int pid =
        ::clone(WorkerImpl, stack + sizeof(stack),
                CLONE_VM | CLONE_VFORK | CLONE_NEWIPC | CLONE_NEWPID | SIGCHLD |
                    CLONE_NEWUTS | CLONE_NEWNS | CLONE_NEWUSER,
                &worker_impl_arg);
    if (pid == -1) {
      PLOG(ERROR) << "clone()";
      break;
    }

    int status;
    if (::waitpid(pid, &status, /*options=*/0) == -1) {
      PLOG(INFO) << "waitpid()";
    }
    if (!WIFEXITED(status)) {
      if (WIFSIGNALED(status)) {
        LOG(INFO) << "Process pid=" << pid
                  << " terminated (signal=" << WTERMSIG(status)
                  << ", coredump=" << WCOREDUMP(status) << ")";
      } else {
        LOG(INFO) << "Process pid=" << pid << " did not exit";
      }
    } else if (const int exit_code = WEXITSTATUS(status); exit_code != 0) {
      LOG(INFO) << "Process pid=" << pid << " exit_code=" << exit_code;
    }
  }
  return 0;
}

// The cmdline (`argv[0]`) of workers is set to the execution_token in the
// `exec` call at the end of `WorkerImpl` so we can kill workers by
// execution_token.
void KillByCmdline(std::string_view cmdline) {
  DIR* const dir = ::opendir("/proc");
  if (dir == nullptr) {
    PLOG(ERROR) << "Failed to open '/proc'";
    return;
  }
  struct ::dirent* entry;
  while ((entry = ::readdir(dir)) != nullptr) {
    if (entry->d_type != DT_DIR) {
      continue;
    }
    const int pid = ::atoi(entry->d_name);
    if (pid <= 0) {
      continue;
    }
    char cmdline_path[32];
    ::snprintf(cmdline_path, sizeof(cmdline_path), "/proc/%d/cmdline", pid);
    FILE* const cmdline_file = ::fopen(cmdline_path, "r");
    if (cmdline_file == nullptr) {
      continue;
    }
    char content[256] = {0};
    (void)::fgets(content, sizeof(content), cmdline_file);
    ::fclose(cmdline_file);
    if (content == cmdline) {
      ::kill(pid, SIGKILL);
      break;
    }
  }
  ::closedir(dir);
}

class WorkerRunner final : public WorkerRunnerService::Service {
 public:
  WorkerRunner(std::string socket_name, std::vector<std::string> mounts,
               std::string log_dir_name, const int dev_null_fd,
               const std::filesystem::path& progdir)
      : socket_name_(std::move(socket_name)),
        mounts_(std::move(mounts)),
        log_dir_name_(std::move(log_dir_name)),
        dev_null_fd_(dev_null_fd),
        progdir_(progdir) {}

  ~WorkerRunner() {
    LOG(INFO) << "Shutting down.";

    // Kill extant workers before exit.
    for (const auto& [_, pids] : code_token_to_reloader_pids_) {
      for (const int pid : pids) {
        if (::killpg(pid, SIGKILL) == -1) {
          // If the group has already terminated, degrade error to a log.
          if (errno == ESRCH) {
            PLOG(INFO) << "killpg(" << pid << ", SIGKILL)";
          } else {
            PLOG(ERROR) << "killpg(" << pid << ", SIGKILL)";
          }
        }
        while (::waitpid(-pid, /*wstatus=*/nullptr, /*options=*/0) > 0) {
        }
      }
    }
  }

  grpc::Status LoadBinary(grpc::ServerContext* /*context*/,
                          const LoadBinaryRequest* request,
                          LoadBinaryResponse* /*response*/)
      ABSL_LOCKS_EXCLUDED(mu_) {
    return privacy_sandbox::server_common::FromAbslStatus(Load(*request));
  }

  grpc::Status DeleteBinary(grpc::ServerContext* /*context*/,
                            const DeleteBinaryRequest* request,
                            DeleteBinaryResponse* /*response*/)
      ABSL_LOCKS_EXCLUDED(mu_) {
    Delete(request->code_token());
    return grpc::Status::OK;
  }

  grpc::Status Cancel(grpc::ServerContext* /*context*/,
                      const CancelRequest* request,
                      CancelResponse* /*response*/) {
    KillByCmdline(request->execution_token());
    return grpc::Status::OK;
  }

 private:
  struct PidAndExecutionToken {
    int pid;
    std::string execution_token;
  };

  absl::Status Load(const LoadBinaryRequest& request) ABSL_LOCKS_EXCLUDED(mu_) {
    const std::filesystem::path binary_dir = progdir_ / request.code_token();
    if (std::error_code ec;
        !std::filesystem::create_directory(binary_dir, ec)) {
      return absl::InternalError(absl::StrCat(
          "Failed to create ", binary_dir.native(), ": ", ec.message()));
    }
    std::filesystem::path binary_path = binary_dir / *kBinaryExe;
    if (request.has_binary_content()) {
      PS_RETURN_IF_ERROR(SaveNewBinary(binary_path, request.binary_content()));
    } else if (request.has_source_bin_code_token()) {
      PS_RETURN_IF_ERROR(CreateHardLinkForExistingBinary(
          binary_path, request.source_bin_code_token()));
    } else {
      return absl::InternalError("Failed to load binary");
    }
    return CreateWorkerPool(std::move(binary_path), request.code_token(),
                            request.num_workers(), request.enable_log_egress());
  }

  void Delete(std::string_view request_code_token) ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    if (const auto it = code_token_to_reloader_pids_.find(request_code_token);
        it != code_token_to_reloader_pids_.end()) {
      // Kill all workers and delete binary.
      for (const int pid : it->second) {
        if (::killpg(pid, SIGKILL) == -1) {
          PLOG(INFO) << "killpg(" << pid << ", SIGKILL)";
        }
        while (::waitpid(-pid, /*wstatus=*/nullptr, /*options=*/0) > 0) {
        }
      }
      code_token_to_reloader_pids_.erase(it);
      const std::filesystem::path binary_dir = progdir_ / request_code_token;
      if (std::error_code ec; std::filesystem::remove_all(binary_dir, ec) ==
                              static_cast<std::uintmax_t>(-1)) {
        LOG(ERROR) << "Failed to remove " << binary_dir << ": " << ec;
      }
    }
  }

  absl::Status SaveNewBinary(const std::filesystem::path& binary_path,
                             std::string_view binary_content) {
    const int fd =
        ::open(binary_path.c_str(), O_WRONLY | O_CREAT | O_CLOEXEC, 0500);
    if (fd == -1) {
      return absl::ErrnoToStatus(errno, "open()");
    }
    if (::write(fd, binary_content.data(), binary_content.size()) !=
        binary_content.size()) {
      return absl::ErrnoToStatus(errno, "write()");
    }

    // Flush the file to ensure the executable is closed for writing before
    // the `exec` call.
    if (::fsync(fd) == -1) {
      return absl::ErrnoToStatus(errno, "fsync()");
    }
    if (::close(fd) == -1) {
      return absl::ErrnoToStatus(errno, "close()");
    }
    return absl::OkStatus();
  }

  absl::Status CreateHardLinkForExistingBinary(
      const std::filesystem::path& binary_path,
      std::string_view source_bin_code_token) {
    const std::filesystem::path existing_binary_path =
        progdir_ / source_bin_code_token / *kBinaryExe;
    if (!std::filesystem::exists(existing_binary_path)) {
      return absl::FailedPreconditionError(absl::StrCat(
          "Expected binary ", existing_binary_path.native(), " not found"));
    } else if (!std::filesystem::is_regular_file(existing_binary_path)) {
      return absl::InternalError(absl::StrCat(
          "File ", existing_binary_path.native(), " not a regular file"));
    }
    std::filesystem::create_hard_link(existing_binary_path, binary_path);
    return absl::OkStatus();
  }

  absl::Status CreateWorkerPool(std::filesystem::path binary_path,
                                std::string_view code_token,
                                const int num_workers,
                                const bool enable_log_egress)
      ABSL_LOCKS_EXCLUDED(mu_) {
    {
      absl::MutexLock lock(&mu_);
      code_token_to_reloader_pids_[code_token].reserve(num_workers);
    }
    std::vector<std::string> mounts = mounts_;
    mounts.push_back(binary_path.parent_path());
    if (enable_log_egress) {
      mounts.push_back(log_dir_name_);
    }
    ReloaderImplArg reloader_impl_arg{
        .mounts = std::move(mounts),
        .socket_name = socket_name_,
        .code_token = std::string(code_token),
        .binary_path = std::move(binary_path),
        .dev_null_fd = dev_null_fd_,
        .enable_log_egress = enable_log_egress,
        .log_dir_name = log_dir_name_,
    };
    for (int i = 0; i < num_workers; ++i) {
      // TODO: b/375622989 - Refactor run_workers to make the code more
      // coherent.
      alignas(16) char stack[1 << 20];
      const pid_t pid = ::clone(ReloaderImpl, stack + sizeof(stack),
                                CLONE_NEWNS | SIGCHLD, &reloader_impl_arg);
      if (pid == -1) {
        return absl::ErrnoToStatus(errno, "clone()");
      }
      absl::MutexLock lock(&mu_);
      code_token_to_reloader_pids_[code_token].push_back(pid);
    }
    return absl::OkStatus();
  }

  const std::string socket_name_;
  const std::vector<std::string> mounts_;
  const std::string log_dir_name_;
  const int dev_null_fd_;
  const std::filesystem::path& progdir_;
  absl::Mutex mu_;
  absl::flat_hash_map<std::string, std::vector<int>>
      code_token_to_reloader_pids_ ABSL_GUARDED_BY(mu_);
};

ABSL_CONST_INIT absl::Mutex signal_mu{absl::kConstInit};
bool signal_flag = false;
}  // namespace

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();
  LOG(INFO) << "Starting up.";
  const std::filesystem::path prog_dir = "/prog_dir";
  if (std::error_code ec; !std::filesystem::create_directories(prog_dir, ec)) {
    LOG(ERROR) << "Failed to create " << prog_dir << ": " << ec;
    return -1;
  }
  absl::Cleanup progdir_cleanup = [&prog_dir] {
    if (absl::Status status =
            ::privacy_sandbox::server_common::byob::RemoveDirectories(prog_dir);
        !status.ok()) {
      LOG(ERROR) << status;
    }
  };
  const int dev_null_fd = ::open("/dev/null", O_WRONLY);
  if (dev_null_fd < 0) {
    PLOG(ERROR) << "open failed for /dev/null";
    return -1;
  }
  WorkerRunner runner(absl::GetFlag(FLAGS_udf_socket_name),
                      absl::GetFlag(FLAGS_mounts), absl::GetFlag(FLAGS_log_dir),
                      dev_null_fd, prog_dir);
  grpc::EnableDefaultHealthCheckService(true);
  std::unique_ptr<grpc::Server> server =
      grpc::ServerBuilder()
          .AddListeningPort(
              absl::StrCat("unix:", absl::GetFlag(FLAGS_control_socket_name)),
              grpc::InsecureServerCredentials())
          .SetMaxSendMessageSize(200'000'000)
          .SetMaxReceiveMessageSize(200'000'000)
          .AddChannelArgument(GRPC_ARG_KEEPALIVE_TIME_MS,
                              absl::ToInt64Milliseconds(absl::Minutes(10)))
          .AddChannelArgument(GRPC_ARG_KEEPALIVE_TIMEOUT_MS,
                              absl::ToInt64Milliseconds(absl::Seconds(20)))
          // TODO(b/378743613): Failed to set zerocopy options on socket for
          // non-Gvisor
          .AddChannelArgument(GRPC_ARG_TCP_TX_ZEROCOPY_ENABLED, 0)
          .RegisterService(&runner)
          .BuildAndStart();
  server->GetHealthCheckService()->SetServingStatus(true);

  // Run until `SIGTERM` received but then exit normally.
  ::signal(
      SIGTERM, +[](int /*sig*/) {
        absl::MutexLock lock(&signal_mu);
        signal_flag = true;
      });
  {
    absl::MutexLock lock(&signal_mu);
    signal_mu.Await(absl::Condition(&signal_flag));
  }
  server->GetHealthCheckService()->SetServingStatus(false);
}
