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

namespace google::scp::roma::benchmark::compiler {

constexpr auto kTimeout = absl::Seconds(10);

std::vector<std::vector<std::string>> CreateOptimizerCombinations();
const std::vector<std::string> kOptimizerOpts = {
    "--turbofan",
    "--maglev",
    "--turboshaft",
};
const std::vector<std::vector<std::string>> kOptimizerCombos =
    CreateOptimizerCombinations();

/**
 * Recursive helper function for CreateOptimizerCombinations.
 *
 * @param curr_combination The current partial combination of optimizer flags.
 * @param index The index in kOptimizerOpts for the next flag to consider.
 * @param combinations The list of all combinations discovered so far (updated
 * by reference).
 */
void CreateOptimizerCombinationsHelper(
    const std::vector<std::string>& curr_combination, int index,
    std::vector<std::vector<std::string>>& combinations) {
  // Base case: All flags from kOptimizerOpts processed
  if (index == kOptimizerOpts.size()) {
    if (curr_combination.empty()) {
      combinations.push_back({"--no-turbofan"});
    } else {
      combinations.push_back(curr_combination);
    }
    return;
  }

  // Recursive step: Explore two paths

  // 1. Skip the current flag (don't include it in the combination)
  CreateOptimizerCombinationsHelper(curr_combination, index + 1, combinations);

  // 2. Include the current flag
  std::vector<std::string> new_combination =
      curr_combination;  // Create new combination to contain flag.
  new_combination.push_back(kOptimizerOpts[index]);  // Add current flag
  CreateOptimizerCombinationsHelper(new_combination, index + 1, combinations);
}

/**
 * Creates all possible combinations of optimizer flags from the global list
 * kOptimizerOpts. Each combination is a vector of flag strings (e.g.,
 * {"--flag1", "--flag2"}). If no optimizer flags are present in a combination,
 * it includes "--no-turbofan" instead as a "default" value.
 *
 * This function uses a recursive helper function to build combinations
 * incrementally.
 */
std::vector<std::vector<std::string>> CreateOptimizerCombinations() {
  std::vector<std::vector<std::string>> optimizer_combos;
  CreateOptimizerCombinationsHelper({}, 0, optimizer_combos);
  return optimizer_combos;
}

}  // namespace google::scp::roma::benchmark::compiler

#endif  // ROMA_BENCHMARK_COMPILER_COMPILER_UTILS_H_
