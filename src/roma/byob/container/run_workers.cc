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
#include <seccomp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/capability.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/seccomp.h>

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
#include "src/roma/byob/dispatcher/dispatcher.grpc.pb.h"
#include "src/roma/byob/dispatcher/dispatcher.pb.h"
#include "src/roma/byob/dispatcher/interface.h"
#include "src/roma/byob/utility/utils.h"
#include "src/util/status_macro/status_macros.h"
#include "src/util/status_macro/status_util.h"

ABSL_FLAG(std::string, control_socket_name, "/sock_dir/xyzw.sock",
          "Socket for host to run_workers communication");
ABSL_FLAG(std::string, udf_socket_name, "/sock_dir/abcd.sock",
          "Socket for host to UDF communication");
ABSL_FLAG(std::vector<std::string>, mounts,
          std::vector<std::string>({"/lib", "/lib64"}),
          "Mounts containing dependencies needed by the binary");
ABSL_FLAG(std::string, log_dir, "/log_dir", "Directory used for storing logs");
ABSL_FLAG(bool, enable_seccomp_filter, false,
          "Decides whether seccomp filtering should be applied.");

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

constexpr std::array<int, 121> kSyscallAllowlist = {
    SCMP_SYS(arch_prctl),
    SCMP_SYS(brk),
    // Only needed by cap_udf test to verify no capabilities are available to
    // the binary.
    SCMP_SYS(capget),
    SCMP_SYS(clone),
    SCMP_SYS(clone3),
    SCMP_SYS(close),
    SCMP_SYS(connect),
    SCMP_SYS(dup2),
    SCMP_SYS(dup3),
    SCMP_SYS(epoll_create),
    SCMP_SYS(epoll_create1),
    SCMP_SYS(epoll_ctl),
    SCMP_SYS(epoll_ctl_old),
    SCMP_SYS(epoll_pwait),
    SCMP_SYS(epoll_wait),
    SCMP_SYS(epoll_wait_old),
    SCMP_SYS(execve),
    SCMP_SYS(execveat),
    SCMP_SYS(exit),
    SCMP_SYS(exit_group),
    SCMP_SYS(fstat),
    SCMP_SYS(fstat64),
    SCMP_SYS(fstatat64),
    SCMP_SYS(fstatfs),
    SCMP_SYS(fstatfs64),
    SCMP_SYS(fsync),
    SCMP_SYS(futex),
    SCMP_SYS(futex_time64),
    SCMP_SYS(futimesat),
    SCMP_SYS(getdents),
    SCMP_SYS(getdents64),
    SCMP_SYS(getegid),
    SCMP_SYS(getegid32),
    SCMP_SYS(geteuid),
    SCMP_SYS(geteuid32),
    SCMP_SYS(getgid),
    SCMP_SYS(getgid32),
    SCMP_SYS(getgroups),
    SCMP_SYS(getgroups32),
    SCMP_SYS(getpgid),
    SCMP_SYS(getpgrp),
    SCMP_SYS(getpid),
    SCMP_SYS(getpmsg),
    SCMP_SYS(getppid),
    SCMP_SYS(getpriority),
    SCMP_SYS(getrandom),
    SCMP_SYS(getresgid),
    SCMP_SYS(getresgid32),
    SCMP_SYS(getresuid),
    SCMP_SYS(getresuid32),
    SCMP_SYS(getrlimit),
    SCMP_SYS(getrusage),
    SCMP_SYS(getsid),
    SCMP_SYS(getsockname),
    SCMP_SYS(getsockopt),
    SCMP_SYS(getuid),
    SCMP_SYS(getuid32),
    SCMP_SYS(getxattr),
    SCMP_SYS(link),
    SCMP_SYS(linkat),
    SCMP_SYS(lock),
    SCMP_SYS(lseek),
    SCMP_SYS(lsetxattr),
    SCMP_SYS(lstat),
    SCMP_SYS(lstat64),
    SCMP_SYS(madvise),
    SCMP_SYS(mmap),
    SCMP_SYS(mmap2),
    SCMP_SYS(mount),
    SCMP_SYS(mprotect),
    SCMP_SYS(mremap),
    SCMP_SYS(munmap),
    SCMP_SYS(name_to_handle_at),
    SCMP_SYS(newfstatat),
    SCMP_SYS(open),
    SCMP_SYS(openat),
    // Needed by the pause_udf for testing.
    SCMP_SYS(pause),
    SCMP_SYS(poll),
    SCMP_SYS(ppoll),
    SCMP_SYS(ppoll_time64),
    SCMP_SYS(prctl),
    SCMP_SYS(pread64),
    SCMP_SYS(preadv),
    SCMP_SYS(preadv2),
    SCMP_SYS(prlimit64),
    SCMP_SYS(pwrite64),
    SCMP_SYS(pwritev),
    SCMP_SYS(pwritev2),
    SCMP_SYS(read),
    SCMP_SYS(readlink),
    SCMP_SYS(readlinkat),
    SCMP_SYS(readv),
    SCMP_SYS(restart_syscall),
    SCMP_SYS(rt_sigaction),
    SCMP_SYS(rt_sigpending),
    SCMP_SYS(rt_sigprocmask),
    SCMP_SYS(rt_sigreturn),
    SCMP_SYS(rt_sigsuspend),
    SCMP_SYS(rt_sigtimedwait),
    SCMP_SYS(rt_sigtimedwait_time64),
    SCMP_SYS(sched_getaffinity),
    SCMP_SYS(sched_yield),
    SCMP_SYS(set_robust_list),
    SCMP_SYS(set_tid_address),
    SCMP_SYS(setsockopt),
    SCMP_SYS(sigaltstack),
    SCMP_SYS(socket),
    SCMP_SYS(stat),
    SCMP_SYS(stat64),
    SCMP_SYS(wait4),
    SCMP_SYS(waitpid),
    SCMP_SYS(umount2),
    SCMP_SYS(uname),
    SCMP_SYS(unshare),
    SCMP_SYS(unlink),
    SCMP_SYS(unlinkat),
    SCMP_SYS(write),
    // Added for generate_bid_bin
    SCMP_SYS(clock_gettime),
    SCMP_SYS(gettid),
    SCMP_SYS(nanosleep),
    SCMP_SYS(rseq),
};

std::string GenerateUuid() {
  uuid_t uuid;
  uuid_generate(uuid);
  std::string uuid_cstr(kNumTokenBytes, '\0');
  uuid_unparse(uuid, uuid_cstr.data());
  return uuid_cstr;
}

// Creates and returns a scmp_filter_ctx which allowlists specific syscalls and
// causes an EPERM for syscalls not on the allowlist.
absl::StatusOr<scmp_filter_ctx> GetSeccompFilter() {
  scmp_filter_ctx ctx;
  if ((ctx = seccomp_init(SCMP_ACT_ERRNO(EPERM))) == NULL) {
    return absl::ErrnoToStatus(errno, "Failed to seccomp_init");
  }
  for (const auto& syscall : kSyscallAllowlist) {
    if (seccomp_rule_add(ctx, SCMP_ACT_ALLOW, syscall, /*arg_cnt=*/0) < 0) {
      seccomp_release(ctx);
      return absl::ErrnoToStatus(
          errno, absl::StrCat("Failed to seccomp_rule_add ", syscall));
    }
  }
  return ctx;
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
      sources_and_targets_read_and_write;
  std::string_view execution_token;
  std::string_view socket_name;
  std::string_view code_token;
  const std::filesystem::path& binary_path;
  int dev_null_fd;
  bool enable_log_egress;
  std::string_view log_dir_name;
  std::string_view socket_dir_name;
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
  // socket_dir is mounted read-only but must not be exposed to the udf.
  if (::umount2(worker_impl_arg.socket_dir_name.data(), MNT_DETACH) == -1) {
    return absl::ErrnoToStatus(
        errno,
        absl::StrCat("Failed to umount ", worker_impl_arg.socket_dir_name));
  }
  // Read/Write mounted directories must not be expsed to the udf.
  for (const auto& [_, target] :
       worker_impl_arg.sources_and_targets_read_and_write) {
    if (::umount2(target.c_str(), MNT_DETACH) == -1) {
      return absl::ErrnoToStatus(
          errno, absl::StrCat("umount2(", target.native(), ")"));
    }
  }
  // Root directory and any umounted targets must be read-only.
  PS_RETURN_IF_ERROR(::privacy_sandbox::server_common::byob::Mount(
      "/", "/", nullptr, MS_REMOUNT | MS_BIND | MS_RDONLY));
  if (::unshare(CLONE_NEWUSER) == -1) {
    return absl::ErrnoToStatus(errno, "unshare(CLONE_NEWUSER)");
  }
  return absl::OkStatus();
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
  std::filesystem::path pivot_root_dir;
  int dev_null_fd;
  bool enable_log_egress;
  bool enable_seccomp_filter;
  std::string log_dir_name;
};

std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
GetSourcesAndTargets(absl::Span<const std::string> mounts) {
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets;
  sources_and_targets.reserve(mounts.size());
  for (const std::filesystem::path mount : mounts) {
    // Mount /x -> /x and /y/z -> /z.
    sources_and_targets.push_back(
        {mount,
         "/" /
             (mount.has_filename() ? mount : mount.parent_path()).filename()});
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
  const std::filesystem::path socket_dir =
      std::filesystem::path(reloader_impl_arg.socket_name).parent_path();
  std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
      sources_and_targets_read_and_write;
  if (reloader_impl_arg.enable_log_egress) {
    sources_and_targets_read_and_write.emplace_back(
        reloader_impl_arg.log_dir_name, reloader_impl_arg.log_dir_name);
  }
  {
    std::vector<std::pair<std::filesystem::path, std::filesystem::path>>
        sources_and_targets_read_only =
            GetSourcesAndTargets(reloader_impl_arg.mounts);
    sources_and_targets_read_only.push_back({socket_dir, socket_dir});

    // SetupPivotRoot reduces the base filesystem image size. This pivot root
    // includes the socket_dir, which must not be shared with the pivot_root
    // created by the worker.
    CHECK_OK(::privacy_sandbox::server_common::byob::SetupPivotRoot(
        reloader_impl_arg.pivot_root_dir, sources_and_targets_read_only,
        /*cleanup_pivot_root_dir=*/true,
        /*sources_and_targets_read_and_write=*/
        {{reloader_impl_arg.log_dir_name, reloader_impl_arg.log_dir_name}},
        // Cannot remount root as read-only since creation of per-worker
        // pivot_root needs write permissions.
        /*remount_root_as_read_only=*/false));
  }
  CHECK_OK(SetPrctlOptions({{PR_CAPBSET_DROP, CAP_SYS_BOOT},
                            {PR_CAPBSET_DROP, CAP_SYS_MODULE},
                            {PR_CAPBSET_DROP, CAP_SYS_RAWIO},
                            {PR_CAPBSET_DROP, CAP_MKNOD},
                            {PR_CAPBSET_DROP, CAP_NET_ADMIN}}));
  absl::StatusOr<scmp_filter_ctx> scmp_filter;
  if (reloader_impl_arg.enable_seccomp_filter) {
    scmp_filter = GetSeccompFilter();
    CHECK_OK(scmp_filter);
    PCHECK(seccomp_load(*scmp_filter) == 0);
  }
  while (true) {
    // Start a new worker.
    const std::string execution_token = GenerateUuid();
    WorkerImplArg worker_impl_arg{
        .pivot_root_dir = reloader_impl_arg.pivot_root_dir,
        .sources_and_targets_read_and_write =
            sources_and_targets_read_and_write,
        .execution_token = execution_token,
        .socket_name = reloader_impl_arg.socket_name,
        .code_token = reloader_impl_arg.code_token,
        .binary_path = reloader_impl_arg.binary_path,
        .dev_null_fd = reloader_impl_arg.dev_null_fd,
        .enable_log_egress = reloader_impl_arg.enable_log_egress,
        .log_dir_name = reloader_impl_arg.log_dir_name,
        .socket_dir_name = socket_dir.native(),
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
                    CLONE_NEWUTS | CLONE_NEWNS,
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
  if (reloader_impl_arg.enable_seccomp_filter) {
    seccomp_release(*scmp_filter);
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
               const std::filesystem::path& prog_dir)
      : socket_name_(std::move(socket_name)),
        mounts_(std::move(mounts)),
        log_dir_name_(std::move(log_dir_name)),
        dev_null_fd_(dev_null_fd),
        prog_dir_(prog_dir) {}

  ~WorkerRunner() {
    LOG(INFO) << "Shutting down.";

    // Kill extant workers before exit.
    for (const auto& [_, pids] : code_token_to_reloader_metadata_) {
      for (const auto& [pid, pivot_root_dir] : pids) {
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
        if (absl::Status status =
                ::privacy_sandbox::server_common::byob::RemoveDirectories(
                    pivot_root_dir);
            !status.ok()) {
          LOG(ERROR) << "Failed to delete pivot_root directory "
                     << pivot_root_dir << ": " << status;
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
    const std::filesystem::path binary_dir = prog_dir_ / request.code_token();
    if (std::error_code ec;
        !std::filesystem::create_directory(binary_dir, ec)) {
      return absl::InternalError(absl::StrCat(
          "Failed to create ", binary_dir.native(), ": ", ec.message()));
    }
    std::error_code ec;
    if (std::filesystem::permissions(binary_dir,
                                     std::filesystem::perms::owner_all, ec);
        ec) {
      return absl::InternalError(
          absl::StrCat("Failed to modify permissions for ", binary_dir.native(),
                       ": ", ec.message()));
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
                            request.num_workers(), request.enable_log_egress(),
                            absl::GetFlag(FLAGS_enable_seccomp_filter));
  }

  void Delete(std::string_view request_code_token) ABSL_LOCKS_EXCLUDED(mu_) {
    absl::MutexLock lock(&mu_);
    if (const auto it =
            code_token_to_reloader_metadata_.find(request_code_token);
        it != code_token_to_reloader_metadata_.end()) {
      // Kill all workers and delete binary.
      for (const auto& [pid, pivot_root_dir] : it->second) {
        if (::killpg(pid, SIGKILL) == -1) {
          PLOG(INFO) << "killpg(" << pid << ", SIGKILL)";
        }
        while (::waitpid(-pid, /*wstatus=*/nullptr, /*options=*/0) > 0) {
        }
        if (absl::Status status =
                ::privacy_sandbox::server_common::byob::RemoveDirectories(
                    pivot_root_dir);
            !status.ok()) {
          LOG(ERROR) << "Failed to delete pivot_root directory "
                     << pivot_root_dir << ": " << status;
        }
      }
      code_token_to_reloader_metadata_.erase(it);
      const std::filesystem::path binary_dir = prog_dir_ / request_code_token;
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
        prog_dir_ / source_bin_code_token / *kBinaryExe;
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

  absl::Status CreateWorkerPool(const std::filesystem::path binary_path,
                                std::string_view code_token,
                                const int num_workers,
                                const bool enable_log_egress,
                                const bool enable_seccomp_filter)
      ABSL_LOCKS_EXCLUDED(mu_) {
    {
      absl::MutexLock lock(&mu_);
      code_token_to_reloader_metadata_[code_token].reserve(num_workers);
    }
    std::vector<std::string> mounts = mounts_;
    const std::filesystem::path binary_dir = binary_path.parent_path();
    mounts.push_back(binary_dir);
    ReloaderImplArg reloader_impl_arg{
        .mounts = std::move(mounts),
        .socket_name = socket_name_,
        .code_token = std::string(code_token),
        // Within the pivot root, binary_dir is a child of root, not prog_dir.
        .binary_path = binary_dir.filename() / binary_path.filename(),
        .dev_null_fd = dev_null_fd_,
        .enable_log_egress = enable_log_egress,
        .enable_seccomp_filter = enable_seccomp_filter,
        .log_dir_name = log_dir_name_,
    };
    for (int i = 0; i < num_workers; ++i) {
      PS_ASSIGN_OR_RETURN(std::filesystem::path pivot_root_dir,
                          CreatePivotRootDir());
      reloader_impl_arg.pivot_root_dir = pivot_root_dir;
      // TODO: b/375622989 - Refactor run_workers to make the code more
      // coherent.
      alignas(16) char stack[1 << 20];
      const pid_t pid = ::clone(ReloaderImpl, stack + sizeof(stack),
                                CLONE_NEWNS | SIGCHLD, &reloader_impl_arg);
      if (pid == -1) {
        return absl::ErrnoToStatus(errno, "clone()");
      }
      absl::MutexLock lock(&mu_);
      code_token_to_reloader_metadata_[code_token].push_back(
          {pid, std::move(pivot_root_dir)});
    }
    return absl::OkStatus();
  }

  const std::string socket_name_;
  const std::vector<std::string> mounts_;
  const std::string log_dir_name_;
  const int dev_null_fd_;
  const std::filesystem::path& prog_dir_;
  absl::Mutex mu_;
  // Maps the code token to the corresponding reloaders' pids and
  // pivot_root_dirs.
  absl::flat_hash_map<std::string,
                      std::vector<std::pair<int, std::filesystem::path>>>
      code_token_to_reloader_metadata_ ABSL_GUARDED_BY(mu_);
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
  std::error_code ec;
  if (std::filesystem::permissions(prog_dir, std::filesystem::perms::owner_all,
                                   ec);
      ec) {
    LOG(ERROR) << "Failed to modify permission for " << prog_dir << ": " << ec;
    return -1;
  }
  absl::Cleanup prog_dir_cleanup = [&prog_dir] {
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
