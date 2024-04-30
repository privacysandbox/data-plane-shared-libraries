/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef ROMA_BENCHMARK_COMPILER_COMPILER_UTILS_H_
#define ROMA_BENCHMARK_COMPILER_COMPILER_UTILS_H_

#include <string>
#include <vector>

#include "absl/time/time.h"
#include "src/roma/config/config.h"

namespace google::scp::roma::benchmark::compiler {

using google::scp::roma::V8CompilerOptions;

constexpr auto kTimeout = absl::Seconds(10);

std::vector<V8CompilerOptions> CreateOptimizerCombinations();
const std::vector<V8CompilerOptions> kOptimizerCombos =
    CreateOptimizerCombinations();

/**
 * Creates all possible combinations of optimizer flags from the global list
 * kOptimizerOpts. Each combination is a set of flag strings (e.g.,
 * {"--flag1", "--flag2"}). If no optimizer flags are present in a combination,
 * it includes "--no-turbofan" instead as a "default" value.
 *
 * This function uses a recursive helper function to build combinations
 * incrementally.
 */
std::vector<V8CompilerOptions> CreateOptimizerCombinations() {
  std::vector<V8CompilerOptions> options;

  for (bool turbofan : {false, true}) {
    for (bool maglev : {false, true}) {
      for (bool turboshaft : {false, true}) {
        options.push_back({turbofan, maglev, turboshaft});
      }
    }
  }
  return options;
}

}  // namespace google::scp::roma::benchmark::compiler

#endif  // ROMA_BENCHMARK_COMPILER_COMPILER_UTILS_H_
