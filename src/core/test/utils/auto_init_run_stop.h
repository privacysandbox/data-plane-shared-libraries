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

#include "src/public/core/test_execution_result_matchers.h"

namespace google::scp::core::test {

// Automatically calls target service's Init()/Run()/Stop() with RAII semantics.
template <typename Service>
struct AutoInitRunStop {
  explicit AutoInitRunStop(Service& service) : service_(service) {
    EXPECT_SUCCESS(service_.Init());
    EXPECT_SUCCESS(service_.Run());
  }

  ~AutoInitRunStop() { EXPECT_SUCCESS(service_.Stop()); }

  Service& service_;
};

// Variant of AutoInitRunStop that works with absl::Status instead of
// ExecutionResult.
template <typename Service>
struct AutoInitRunStopStatus {
  explicit AutoInitRunStopStatus(Service& service) : service_(service) {
    EXPECT_TRUE(service_.Init().ok());
    EXPECT_TRUE(service_.Run().ok());
  }

  ~AutoInitRunStopStatus() { EXPECT_TRUE(service_.Stop().ok()); }

  Service& service_;
};

}  // namespace google::scp::core::test
