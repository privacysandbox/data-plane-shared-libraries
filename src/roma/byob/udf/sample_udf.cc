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

#include <cmath>
#include <cstring>
#include <iostream>
#include <vector>

#include "absl/log/check.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"
#include "absl/strings/numbers.h"
#include "google/protobuf/any.pb.h"
#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/host/callback.pb.h"
#include "src/roma/byob/udf/sample.pb.h"

using ::google::protobuf::io::FileInputStream;
using ::google::protobuf::util::ParseDelimitedFromZeroCopyStream;
using ::google::protobuf::util::SerializeDelimitedToFileDescriptor;
using ::privacy_sandbox::server_common::byob::Callback;
using ::privacy_sandbox::server_common::byob::FUNCTION_CALLBACK;
using ::privacy_sandbox::server_common::byob::FUNCTION_HELLO_WORLD;
using ::privacy_sandbox::server_common::byob::FUNCTION_PRIME_SIEVE;
using ::privacy_sandbox::server_common::byob::FUNCTION_TEN_CALLBACK_INVOCATIONS;
using ::privacy_sandbox::server_common::byob::SampleRequest;
using ::privacy_sandbox::server_common::byob::SampleResponse;

// Find all prime numbers less than this:
constexpr int kPrimeCount = 100'000;

void RunHelloWorld(SampleResponse& bin_response) {
  bin_response.set_greeting("Hello, world!");
}

void RunPrimeSieve(SampleResponse& bin_response) {
  // Create a boolean array of size n+1
  std::vector<bool> primes(kPrimeCount + 1, true);
  // Set first two values to false
  primes[0] = false;
  primes[1] = false;
  // Loop through the elements
  for (int i = 2; i <= sqrt(kPrimeCount); i++) {
    if (primes[i]) {
      for (int j = i * i; j <= kPrimeCount; j += i) {
        primes[j] = false;
      }
    }
  }
  // Loop through the array from 2 to n
  for (int i = 2; i <= kPrimeCount; i++) {
    if (primes[i]) {
      bin_response.add_prime_number(i);
    }
  }
}

void RunEchoCallback(int fd) {
  {
    Callback callback;
    callback.set_function_name("example");
    google::protobuf::Any any;
    CHECK(any.PackFrom(std::move(callback)));
    CHECK(SerializeDelimitedToFileDescriptor(any, fd));
  }
  Callback callback;
  FileInputStream input(fd);
  CHECK(ParseDelimitedFromZeroCopyStream(&callback, &input, nullptr));
}

int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 2) {
    std::cerr << "Not enough arguments!";
    return -1;
  }
  int32_t fd;
  CHECK(absl::SimpleAtoi(argv[1], &fd))
      << "Conversion of file descriptor string to int failed";

  SampleRequest bin_request;
  {
    google::protobuf::Any any;
    FileInputStream input(fd);
    ParseDelimitedFromZeroCopyStream(&any, &input, nullptr);
    any.UnpackTo(&bin_request);
  }
  SampleResponse bin_response;
  switch (bin_request.function()) {
    case FUNCTION_HELLO_WORLD:
      RunHelloWorld(bin_response);
      break;
    case FUNCTION_PRIME_SIEVE:
      RunPrimeSieve(bin_response);
      break;
    case FUNCTION_CALLBACK:
      RunEchoCallback(fd);
      break;
    case FUNCTION_TEN_CALLBACK_INVOCATIONS:
      for (int i = 0; i < 10; ++i) {
        RunEchoCallback(fd);
      }
      break;
    default:
      LOG(INFO) << "Invalid function enum.";
  }
  google::protobuf::Any any;
  any.PackFrom(std::move(bin_response));
  SerializeDelimitedToFileDescriptor(any, fd);
  return 0;
}
