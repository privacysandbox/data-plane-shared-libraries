/**
 * Copyright 2025 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @fileoverview Description of this file.
 */

/**
 * @param {number} runs - Number of times to run the prime computation
 * @param {number} cycles - Number of cycles to compute primes in each run
 * @param {number} prime_sieve_limit - Upper limit for finding prime numbers
 * @returns {void} Logs execution time to console
 */
function primes(runs = 1000, cycles = 10, prime_sieve_limit = 10000) {
  // for compatibility with v8_shell and roma_shell
  if (typeof print !== 'function') {
    print = console.log;
  }
  runs = parseInt(runs);
  cycles = parseInt(cycles);
  prime_sieve_limit = parseInt(prime_sieve_limit);
  e = Date.now();
  for (i = 0; i < runs; i++) {
    computeForCycles(cycles, prime_sieve_limit);
  }
  let time = Date.now() - e;
  print('Execution time: ', time);
  return time;
}

function computeForCycles(totalCycles, prime_sieve_limit) {
  let count = 0;
  while (count < totalCycles) {
    sieve(prime_sieve_limit);
    count++;
  }
}

function sieve(n) {
  const primes = new Array(n + 1).fill(true);
  primes[0] = false;
  primes[1] = false;
  for (let i = 2; i <= Math.sqrt(n); i++) {
    if (primes[i]) {
      for (let j = i * i; j <= n; j += i) {
        primes[j] = false;
      }
    }
  }
}
