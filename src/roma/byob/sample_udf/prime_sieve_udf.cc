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

#include <iostream>

#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/sample_udf/sample_udf_interface.pb.h"

namespace {
using ::privacy_sandbox::roma_byob::example::RunPrimeSieveRequest;
using ::privacy_sandbox::roma_byob::example::RunPrimeSieveResponse;

void RunPrimeSieve(int prime_count, RunPrimeSieveResponse& bin_response) {
  // Create a boolean array of size n+1
  std::vector<bool> primes(prime_count + 1, true);

  // Set first two values to false
  primes[0] = false;
  primes[1] = false;

  // Loop through the elements
  for (int i = 2; i <= sqrt(prime_count); i++) {
    if (primes[i]) {
      for (int j = i * i; j <= prime_count; j += i) {
        primes[j] = false;
      }
    }
  }

  // Loop through the array from 2 to n
  for (int i = prime_count; i >= 0; --i) {
    if (primes[i]) {
      bin_response.set_largest_prime(i);
      return;
    }
  }
}

RunPrimeSieveRequest ReadRequestFromFd(int fd) {
  RunPrimeSieveRequest req;
  google::protobuf::io::FileInputStream stream(fd);
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &stream,
                                                           nullptr);
  return req;
}

void WriteResponseToFd(int fd, RunPrimeSieveResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}
}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::cerr << "Not enough arguments!" << std::endl;
    return -1;
  }
  int fd = std::stoi(argv[1]);
  RunPrimeSieveRequest request = ReadRequestFromFd(fd);
  RunPrimeSieveResponse bin_response;
  RunPrimeSieve(request.prime_count(), bin_response);
  WriteResponseToFd(fd, std::move(bin_response));
  return 0;
}
