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

#ifndef ROMA_UTIL_SCOPED_EXECUTION_TOKEN_H_
#define ROMA_UTIL_SCOPED_EXECUTION_TOKEN_H_

#include <string>
#include <string_view>
#include <utility>

#include "absl/functional/any_invocable.h"
#include "src/util/execution_token.h"

namespace google::scp::roma {

/**
 * @brief RAII wrapper for ExecutionToken.
 *
 * This class ensures that metadata associated with an execution token is
 * automatically deleted when the object goes out of scope, unless the token has
 * been explicitly released.
 */
class ScopedExecutionToken {
 public:
  /**
   * @brief Create a new ScopedExecutionToken with a deletion function.
   *
   * @param token The execution token to wrap
   * @param deletion_function Function to call when token is destroyed
   */
  ScopedExecutionToken(
      ExecutionToken token,
      absl::AnyInvocable<void(std::string_view) const> deletion_function)
      : token_(std::move(token)),
        deletion_function_(std::move(deletion_function)),
        deleted_(false) {}

  /**
   * @brief Create a new ScopedExecutionToken from a string value.
   *
   * @param token_value The string value for the token
   * @param deletion_function Function to call when token is destroyed
   */
  ScopedExecutionToken(
      std::string token_value,
      absl::AnyInvocable<void(std::string_view) const> deletion_function)
      : token_{std::move(token_value)},
        deletion_function_(std::move(deletion_function)),
        deleted_(false) {}

  ScopedExecutionToken(const ScopedExecutionToken&) = delete;
  ScopedExecutionToken& operator=(const ScopedExecutionToken&) = delete;

  ScopedExecutionToken(ScopedExecutionToken&& other) noexcept
      : token_(std::move(other.token_)),
        deletion_function_(std::move(other.deletion_function_)),
        deleted_(other.deleted_) {
    other.deleted_ = true;
  }

  ScopedExecutionToken& operator=(ScopedExecutionToken&& other) noexcept {
    if (this != &other) {
      token_ = std::move(other.token_);
      deletion_function_ = std::move(other.deletion_function_);
      deleted_ = other.deleted_;
      other.deleted_ = true;
    }
    return *this;
  }

  ~ScopedExecutionToken() { Cleanup(); }

  ExecutionToken Release() {
    deleted_ = true;
    return token_;
  }

  const ExecutionToken& Get() const { return token_; }

  const std::string& Value() const { return token_.value; }

 private:
  /**
   * @brief Clean up associated metadata if not already deleted
   */
  void Cleanup() {
    if (!deleted_ && deletion_function_) {
      deletion_function_(token_.value);
      deleted_ = true;
    }
  }

  ExecutionToken token_;
  absl::AnyInvocable<void(std::string_view) const> deletion_function_;
  mutable bool
      deleted_;  // mutable to allow marking as deleted in const contexts
};

}  // namespace google::scp::roma

#endif  // ROMA_UTIL_SCOPED_EXECUTION_TOKEN_H_
