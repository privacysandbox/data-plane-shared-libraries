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

#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include "absl/log/log.h"
#include "absl/time/time.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "google/protobuf/util/time_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"
#include "src/util/duration.h"

using ::google::protobuf::io::FileInputStream;
using ::privacy_sandbox::roma_byob::example::FUNCTION_CLONE;
using ::privacy_sandbox::roma_byob::example::FUNCTION_CLONE_WITH_NEW_NS_FLAG;
using ::privacy_sandbox::roma_byob::example::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::roma_byob::example::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::roma_byob::example::SampleRequest;
using ::privacy_sandbox::roma_byob::example::SampleResponse;

// Find all prime numbers less than this:
constexpr int kPrimeCount = 100'000;

void RunHelloWorld(SampleResponse& bin_response) {
  bin_response.set_greeting("Hello, world!");
}

void RunPrimeSieve(SampleRequest& bin_request, SampleResponse& bin_response) {
  int prime_count =
      bin_request.prime_count() > 0 ? bin_request.prime_count() : kPrimeCount;
  int duration_ns = google::protobuf::util::TimeUtil::DurationToNanoseconds(
      bin_request.duration());
  absl::Duration duration = absl::Nanoseconds(duration_ns);
  bool use_duration = duration_ns > 0;
  double sqrt_prime_count = sqrt(prime_count);

  // Create a boolean array of size n+1
  std::vector<bool> primes(prime_count + 1, true);
  // Set first two values to false
  primes[0] = false;
  primes[1] = false;
  // Loop through the elements
  privacy_sandbox::server_common::Stopwatch stopwatch;
  for (int i = 2; i <= sqrt_prime_count; i++) {
    if (primes[i]) {
      for (int j = i * i; j <= prime_count; j += i) {
        primes[j] = false;
      }
    }

    if (use_duration) {
      if (stopwatch.GetElapsedTime() >= duration) {
        break;
      } else if (i + 1 > sqrt_prime_count) {
        i = 1;  // Reset to restart the sieve if time remains
      }
    }
  }

  // Loop through the array from 2 to n
  for (int i = 2; i <= prime_count; i++) {
    if (primes[i]) {
      bin_response.add_prime_number(i);
    }
  }
}

void RunClone(SampleResponse& bin_response) {
  alignas(16) char stack[1 << 20];
  const int pid = ::clone(
      +[](void* /*arg*/) -> int { _exit(EXIT_SUCCESS); }, stack + sizeof(stack),
      SIGCHLD, /*arg=*/nullptr);
  if (pid > 0) {
    ::waitpid(pid, nullptr, 0);
  } else {
    PLOG(FATAL) << "clone()";
  }
}

void RunCloneWithNewNsFlag(SampleResponse& bin_response) {
  alignas(16) char stack[1 << 20];
  const int pid = ::clone(
      +[](void* /*arg*/) -> int { _exit(EXIT_SUCCESS); }, stack + sizeof(stack),
      SIGCHLD | CLONE_NEWNS, /*arg=*/nullptr);
  if (pid > 0) {
    ::waitpid(pid, nullptr, 0);
  } else {
    PLOG(FATAL) << "clone()";
  }
}

SampleRequest ReadRequestFromFd(int fd) {
  SampleRequest req;
  FileInputStream stream(fd);
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &stream,
                                                           nullptr);
  return req;
}

void WriteResponseToFd(int fd, SampleResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}

int main(int argc, char* argv[]) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  if (::write(fd, "a", /*count=*/1) != 1) {
    std::cerr << "Failed to write" << std::endl;
    return -1;
  }
  SampleRequest bin_request = ReadRequestFromFd(fd);
  SampleResponse bin_response;
  switch (bin_request.function()) {
    case FUNCTION_HELLO_WORLD:
      RunHelloWorld(bin_response);
      break;
    case FUNCTION_PRIME_SIEVE:
      RunPrimeSieve(bin_request, bin_response);
      break;
    case FUNCTION_CLONE:
      RunClone(bin_response);
      break;
    case FUNCTION_CLONE_WITH_NEW_NS_FLAG:
      RunCloneWithNewNsFlag(bin_response);
      break;
    default:
      return -1;
  }
  WriteResponseToFd(fd, std::move(bin_response));
  return 0;
}
