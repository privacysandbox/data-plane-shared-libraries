// Copyright 2025 Google LLC
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

#include <fcntl.h>
#include <sched.h>
#include <stdlib.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <linux/capability.h>
#include <linux/prctl.h>

#include <filesystem>
#include <fstream>

#include <benchmark/benchmark.h>

#include "absl/log/check.h"
#include "absl/log/log.h"

namespace {
void BM_Socket(benchmark::State& state) {
  for (auto _ : state) {
    const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
    PCHECK(0 <= fd);
    state.PauseTiming();
    PCHECK(0 == ::close(fd));
    state.ResumeTiming();
  }
}

void BM_Open(benchmark::State& state) {
  for (auto _ : state) {
    const int fd = ::open("/tmp/blah.txt", O_CREAT | O_WRONLY | O_APPEND, 0644);
    PCHECK(0 <= fd);
    state.PauseTiming();
    PCHECK(0 == ::close(fd));
    PCHECK(0 == ::unlink("/tmp/blah.txt"));
    state.ResumeTiming();
  }
}

void BenchmarkImpl(benchmark::State& state, int fn(void*), void* arg) {
  for (auto _ : state) {
    alignas(16) char stack[1 << 20];

    // TODO(b/406251650): Add CLONE_NEWIPC when issue fixed in kokoro.
    const int pid = ::clone(fn, stack + sizeof(stack),
                            SIGCHLD | CLONE_VM | CLONE_VFORK | CLONE_NEWPID |
                                CLONE_NEWUTS | CLONE_NEWNS,
                            /*arg=*/arg);
    if (pid == -1) {
      PLOG(FATAL) << "clone()";
    }
    PCHECK(pid == ::waitpid(pid, nullptr, 0));
  }
}

void BM_Clone(benchmark::State& state) {
  BenchmarkImpl(
      state, +[](void* /*arg*/) -> int { _exit(EXIT_SUCCESS); },
      /*arg=*/nullptr);
}

void BM_CloneThenPrctl(benchmark::State& state) {
  BenchmarkImpl(
      state,
      +[](void* /*arg*/) -> int {
        PCHECK(0 <= ::prctl(PR_CAPBSET_DROP, CAP_SYS_ADMIN));
        PCHECK(0 <= ::prctl(PR_CAPBSET_DROP, CAP_SETPCAP));
        PCHECK(0 <= ::prctl(PR_SET_PDEATHSIG, SIGHUP));
        _exit(EXIT_SUCCESS);
      },
      /*arg=*/nullptr);
}

void BM_CloneThenUmount2(benchmark::State& state) {
  const std::filesystem::path src = "/tmp/test_mount";
  std::error_code ec;
  std::filesystem::create_directory(src, ec);
  CHECK(!ec);
  {
    std::ofstream ofs(src / "blah.txt");
    CHECK(ofs.is_open());
  }
  std::filesystem::path tgt = "/tmp/test_source";
  std::filesystem::create_directory(tgt, ec);
  CHECK(!ec);
  PCHECK(0 == ::mount(src.c_str(), tgt.c_str(), /*filesystemtype=*/nullptr,
                      MS_BIND | MS_RDONLY, nullptr));
  BenchmarkImpl(
      state,
      +[](void* arg) -> int {
        const auto& tgt = *static_cast<const std::filesystem::path*>(arg);
        PCHECK(0 == ::umount2(tgt.c_str(), MNT_DETACH));
        _exit(EXIT_SUCCESS);
      },
      /*arg=*/&tgt);
  PCHECK(0 == ::umount2(tgt.c_str(), MNT_DETACH));
  std::filesystem::remove(tgt, ec);
  CHECK(!ec);
  std::filesystem::remove_all(src, ec);
  CHECK(!ec);
}

void BM_CloneThenMount(benchmark::State& state) {
  BenchmarkImpl(
      state,
      +[](void* /*arg*/) -> int {
        PCHECK(0 == ::mount("/", "/", nullptr, MS_REMOUNT | MS_BIND | MS_RDONLY,
                            nullptr));
        _exit(EXIT_SUCCESS);
      },
      /*arg=*/nullptr);
}
void BM_CloneThenUnshareNewUserFlag(benchmark::State& state) {
  BenchmarkImpl(
      state,
      +[](void* /*arg*/) -> int {
        PCHECK(0 == ::unshare(CLONE_NEWUSER));
        _exit(EXIT_SUCCESS);
      },
      /*arg=*/nullptr);
}

void BM_CloneThenDup2(benchmark::State& state) {
  int dev_null_fd = ::open("/dev/null", O_WRONLY);
  PCHECK(0 <= dev_null_fd);
  BenchmarkImpl(
      state,
      +[](void* arg) -> int {
        const int dev_null_fd = *static_cast<int*>(arg);
        PCHECK(0 <= ::dup2(dev_null_fd, STDOUT_FILENO));
        _exit(EXIT_SUCCESS);
      },
      /*arg=*/&dev_null_fd);
  PCHECK(0 == ::close(dev_null_fd));
}

void BM_CloneThenExec(benchmark::State& state) {
  BenchmarkImpl(
      state,
      +[](void* /*arg*/) -> int {
        const char* argv[] = {"/bin/true", nullptr};
        ::execve(argv[0], const_cast<char* const*>(&argv[0]), nullptr);
        PLOG(FATAL) << "execve()";
      },
      /*arg=*/nullptr);
}
}  // namespace

BENCHMARK(BM_Socket)->UseRealTime();
BENCHMARK(BM_Open)->UseRealTime();
BENCHMARK(BM_Clone)->UseRealTime();
BENCHMARK(BM_CloneThenPrctl)->UseRealTime();
BENCHMARK(BM_CloneThenUmount2)->UseRealTime();
BENCHMARK(BM_CloneThenMount)->UseRealTime();
BENCHMARK(BM_CloneThenUnshareNewUserFlag)->UseRealTime();
BENCHMARK(BM_CloneThenDup2)->UseRealTime();
BENCHMARK(BM_CloneThenExec)->UseRealTime();

BENCHMARK_MAIN();
