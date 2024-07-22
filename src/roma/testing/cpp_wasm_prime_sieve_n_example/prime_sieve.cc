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
#include <iostream>
#include <vector>

#include <emscripten/bind.h>

class PrimeSieveClass {
 public:
  static int PrimeSieve(int limit) {
    // Create a boolean array of size limit+1
    std::vector<bool> primes(limit + 1, true);

    // Set first two values to false
    primes[0] = false;
    primes[1] = false;

    // Loop through the elements
    for (int i = 2; i <= ::sqrt(limit); ++i) {
      if (primes[i]) {
        for (int j = i * i; j <= limit; j += i) {
          primes[j] = false;
        }
      }
    }
    for (int i = limit; i >= 0; --i) {
      if (primes[i]) {
        return i;
      }
    }
    return -1;
  }
};

EMSCRIPTEN_BINDINGS(PrimeSieve) {
  emscripten::class_<PrimeSieveClass>("PrimeSieveClass")
      .constructor<>()
      .class_function("PrimeSieve", &PrimeSieveClass::PrimeSieve);
}
