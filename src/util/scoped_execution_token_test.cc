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

#include "src/util/scoped_execution_token.h"

#include <string>
#include <string_view>

#include "absl/functional/any_invocable.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "src/util/execution_token.h"

namespace google::scp::roma {
namespace {

class ScopedExecutionTokenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    cleanup_count_ = 0;
    cleanup_value_.clear();
  }

  int cleanup_count_;
  std::string cleanup_value_;

  absl::AnyInvocable<void(std::string_view) const> GetCleanupFunction() {
    return [&](std::string_view value) {
      cleanup_count_++;
      cleanup_value_ = value;
    };
  }
};

TEST_F(ScopedExecutionTokenTest, AutoCleanupOnDestruction) {
  {
    ExecutionToken token{"test_token"};
    ScopedExecutionToken scoped_token(std::move(token), GetCleanupFunction());
    EXPECT_EQ(cleanup_count_, 0);
  }
  EXPECT_EQ(cleanup_count_, 1);
  EXPECT_EQ(cleanup_value_, "test_token");
}

TEST_F(ScopedExecutionTokenTest, NoCleanupAfterRelease) {
  {
    ExecutionToken token{"test_token"};
    ScopedExecutionToken scoped_token(std::move(token), GetCleanupFunction());
    EXPECT_EQ(cleanup_count_, 0);

    ExecutionToken released_token = scoped_token.Release();
    EXPECT_EQ(released_token.value, "test_token");
    EXPECT_EQ(cleanup_count_, 0);
  }
  EXPECT_EQ(cleanup_count_, 0);
}

TEST_F(ScopedExecutionTokenTest, GetDoesNotPreventCleanup) {
  {
    ExecutionToken token{"test_token"};
    ScopedExecutionToken scoped_token(std::move(token), GetCleanupFunction());
    EXPECT_EQ(cleanup_count_, 0);

    const ExecutionToken& ref_token = scoped_token.Get();
    EXPECT_EQ(ref_token.value, "test_token");
    EXPECT_EQ(cleanup_count_, 0);
  }
  EXPECT_EQ(cleanup_count_, 1);
  EXPECT_EQ(cleanup_value_, "test_token");
}

TEST_F(ScopedExecutionTokenTest, MoveConstructionTransfersOwnership) {
  {
    ExecutionToken token{"test_token"};
    ScopedExecutionToken original_token(std::move(token), GetCleanupFunction());

    ScopedExecutionToken moved_token(std::move(original_token));
    EXPECT_EQ(cleanup_count_, 0);
  }
  EXPECT_EQ(cleanup_count_, 1);
  EXPECT_EQ(cleanup_value_, "test_token");
}

TEST_F(ScopedExecutionTokenTest, MoveAssignmentTransfersOwnership) {
  {
    ExecutionToken token1{"token1"};
    ExecutionToken token2{"token2"};
    ScopedExecutionToken token_a(std::move(token1), GetCleanupFunction());
    ScopedExecutionToken token_b(std::move(token2), GetCleanupFunction());

    token_a = std::move(token_b);
    EXPECT_EQ(cleanup_count_, 0);
  }
  EXPECT_EQ(cleanup_count_, 1);
  EXPECT_EQ(cleanup_value_, "token2");
}

TEST_F(ScopedExecutionTokenTest, ValueMethodReturnsCorrectString) {
  ExecutionToken token{"test_token"};
  ScopedExecutionToken scoped_token(std::move(token), GetCleanupFunction());

  EXPECT_EQ(scoped_token.Value(), "test_token");
  EXPECT_EQ(cleanup_count_, 0);
}

TEST_F(ScopedExecutionTokenTest, StringConstructorCreatesValidToken) {
  {
    // Create a token directly from a string
    ScopedExecutionToken scoped_token("test_token", GetCleanupFunction());
    EXPECT_EQ(scoped_token.Value(), "test_token");
    EXPECT_EQ(cleanup_count_, 0);

    // The token should be convertible to ExecutionToken
    ExecutionToken token = scoped_token.Release();
    EXPECT_EQ(token.value, "test_token");
  }
  // No cleanup should occur after release
  EXPECT_EQ(cleanup_count_, 0);
}

}  // namespace
}  // namespace google::scp::roma
