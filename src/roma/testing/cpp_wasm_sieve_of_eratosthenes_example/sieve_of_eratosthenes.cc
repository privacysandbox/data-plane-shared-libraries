// Copyright 2023 Google LLC
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

// Find all prime numbers less than this:
constexpr int kLimit = 100'000;

class PrimeClass {
 public:
  static std::vector<int> SieveOfEratosthenes() {
    // Create a boolean array of size kLimit+1
    // Possibly implemented as
    std::vector<bool> primes(kLimit + 1, true);
    int prime_count = kLimit - 1;
    // Set first two values to false
    primes[0] = false;
    primes[1] = false;
    // Loop through the elements
    for (int i = 2; i <= sqrt(kLimit); i++) {
      if (primes[i]) {
        for (int j = i * i; j <= kLimit; j += i) {
          prime_count -= primes[j];
          primes[j] = false;
        }
      }
    }
    std::vector<int> prime_vec(prime_count);
    int prime_vec_i = 0;
    // Loop through the array from 2 to n
    for (int i = 2; i <= kLimit; i++) {
      if (primes[i]) {
        prime_vec[prime_vec_i] = i;
        prime_vec_i++;
      }
    }
    return prime_vec;
  }
};

EMSCRIPTEN_BINDINGS(Prime) {
  emscripten::register_vector<int>("vector<int>");

  emscripten::class_<PrimeClass>("PrimeClass")
      .constructor<>()
      .class_function("SieveOfEratosthenes", &PrimeClass::SieveOfEratosthenes);
}
