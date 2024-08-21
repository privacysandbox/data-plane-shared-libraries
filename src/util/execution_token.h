/*
 * Copyright 2024 Google LLC
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

#ifndef ROMA_INTERFACE_EXECUTION_TOKEN_H_
#define ROMA_INTERFACE_EXECUTION_TOKEN_H_

#include <string>
#include <utility>

namespace google::scp::roma {
struct ExecutionToken {
  explicit ExecutionToken(const std::string& id) : uuid_(id) {}

  bool operator==(const ExecutionToken& other) const {
    return uuid_ == other.uuid_;
  }

  bool operator!=(const ExecutionToken& other) const {
    return uuid_ != other.uuid_;
  }

  template <typename H>
  friend H AbslHashValue(H h, const ExecutionToken& token) {
    return H::combine(std::move(h), token.uuid_);
  }

 private:
  std::string uuid_;
};
}  // namespace google::scp::roma

#endif  // ROMA_INTERFACE_EXECUTION_TOKEN_H_
