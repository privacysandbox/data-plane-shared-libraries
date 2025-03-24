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

#include <sched.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <unistd.h>

#include <benchmark/benchmark.h>

#include "absl/log/log.h"

namespace {
void BM_LatencyForClone(benchmark::State& state) {
  for (auto _ : state) {
    alignas(16) char stack[1 << 20];
    const int pid = ::clone(
        +[](void* /*arg*/) -> int { _exit(EXIT_SUCCESS); },
        stack + sizeof(stack), SIGCHLD, /*arg=*/nullptr);
    if (pid > 0) {
      ::waitpid(pid, nullptr, 0);
    } else {
      PLOG(FATAL) << "clone()";
    }
  }
}

void BM_LatencyForCloneWithNewIpcFlag(benchmark::State& state) {
  for (auto _ : state) {
    alignas(16) char stack[1 << 20];
    const int pid = ::clone(
        +[](void* /*arg*/) -> int { _exit(EXIT_SUCCESS); },
        stack + sizeof(stack), SIGCHLD | CLONE_NEWIPC, /*arg=*/nullptr);
    if (pid > 0) {
      ::waitpid(pid, nullptr, 0);
    } else {
      PLOG(FATAL) << "clone()";
    }
  }
}
}  // namespace

BENCHMARK(BM_LatencyForClone)->UseRealTime();
BENCHMARK(BM_LatencyForCloneWithNewIpcFlag)->UseRealTime();

BENCHMARK_MAIN();
