/*
 * Copyright 2023 Google LLC
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
#include "src/core/test/utils/proto_test_utils.h"
#include "src/logger/request_context_impl_test.h"

namespace privacy_sandbox::server_common::log {
namespace {

using google::scp::core::test::EqualsProto;
using ::testing::ContainsRegex;
using ::testing::IsEmpty;

TEST_F(ConsentedLogTest, LogNotConsented) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      mismatched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_VLOG(kMaxV, *test_instance_) << kLogContent; }),
              ContainsRegex(absl::StrCat("\\(id: 1234\\)[ \t]+", kLogContent)));
  EXPECT_THAT(ReadSs(), IsEmpty());
}

TEST_F(ConsentedLogTest, LogConsented) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_VLOG(kMaxV, *test_instance_) << kLogContent; }),
              ContainsRegex(absl::StrCat("\\(id: 1234\\)[ \t]+", kLogContent)));
  EXPECT_THAT(ReadSs(),
              ContainsRegex(absl::StrCat("\\(id: 1234\\)[ \t]+", kLogContent)));
}

TEST_F(DebugResponseTest, NotLoggedIfNotSet) {
  // mismatched_token_ does not log debug info
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      mismatched_token_, [this]() {
        accessed_debug_info_ = true;
        return &debug_info_;
      });
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_FALSE(accessed_debug_info_);

  // matched_token_ does not log debug info
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}}, matched_token_,
      [this]() {
        accessed_debug_info_ = true;
        return &debug_info_;
      });
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_FALSE(accessed_debug_info_);
}

TEST_F(DebugResponseTest, LoggedIfSet) {
  // debug_info turned on, then log
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      debug_info_config_, [this]() {
        accessed_debug_info_ = true;
        return &debug_info_;
      });
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_THAT(debug_info_.logs(), ElementsAre(ContainsRegex(absl::StrCat(
                                      "\\(id: 1234\\)[ \t]+", kLogContent))));

  // turn off debug info
  accessed_debug_info_ = false;
  debug_info_ = DebugInfo();
  debug_info_config_.set_is_debug_info_in_response(false);
  test_instance_->Update(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      debug_info_config_);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_FALSE(accessed_debug_info_);

  // turn on debug info
  accessed_debug_info_ = false;
  debug_info_ = DebugInfo();
  debug_info_config_.set_is_debug_info_in_response(true);
  test_instance_->Update(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      debug_info_config_);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_THAT(debug_info_.logs(), ElementsAre(ContainsRegex(absl::StrCat(
                                      "\\(id: 1234\\)[ \t]+", kLogContent))));
}

TEST_F(DebugResponseTest, NoDebugInfoProvided) {
  // No DebugInfo is provided but debug_info_config set to true and
  // matched token is passed
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_THAT(ReadSs(),
              ContainsRegex(absl::StrCat("\\(id: 1234\\)[ \t]+", kLogContent)));

  // No DebugInfo is provided but debug_info_config set to true with empty token
  // config
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      debug_info_config_);
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_THAT(ReadSs(), IsEmpty());
}

TEST_F(DebugResponseTest, EventMessageOn) {
  // debug_info turned on, it will store EventMessage
  {
    auto test_instance_event_message =
        std::make_unique<ContextImpl<MockEventMessageProvider>>(
            absl::btree_map<std::string, std::string>{{"id", "1234"}},
            debug_info_config_, [this]() {
              accessed_debug_info_ = true;
              return &debug_info_;
            });
    test_instance_event_message->SetEventMessageField(
        std::string("test gen id"));
    EXPECT_FALSE(accessed_debug_info_);
    test_instance_event_message->ExportEventMessage();
  }
  EXPECT_TRUE(accessed_debug_info_);
  LogContext unpacked_blob;
  debug_info_.event_message().UnpackTo(&unpacked_blob);
  LogContext blob;
  blob.set_generation_id("test gen id");
  EXPECT_THAT(blob, EqualsProto(unpacked_blob));
}

TEST_F(DebugResponseTest, EventMessageOff) {
  // debug_info turned off, it will not store EventMessage
  debug_info_config_.set_is_debug_info_in_response(false);
  {
    auto test_instance_event_message =
        std::make_unique<ContextImpl<MockEventMessageProvider>>(
            absl::btree_map<std::string, std::string>{{"id", "1234"}},
            debug_info_config_, [this]() {
              accessed_debug_info_ = true;
              return &debug_info_;
            });
    test_instance_event_message->SetEventMessageField(
        std::string("test gen id"));
    EXPECT_FALSE(accessed_debug_info_);
    test_instance_event_message->ExportEventMessage();
  }
  EXPECT_FALSE(accessed_debug_info_);
  EXPECT_FALSE(debug_info_.has_event_message());
}

TEST_F(SafePathLogTest, LogMessage) {
  test_instance_ = CreateTestInstance();
  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_VLOG(kMaxV, *test_instance_) << kLogContent; }),
              ContainsRegex(kLogContent));
  EXPECT_THAT(ReadSs(), ContainsRegex(kLogContent));
}

TEST_F(SystemLogTest, LogMessage) {
  EXPECT_THAT(LogWithCapturedStderr([]() {
                PS_VLOG(kMaxV, SystemLogContext::Get()) << kLogContent;
              }),
              ContainsRegex(kLogContent));
  EXPECT_THAT(ReadSs(), ContainsRegex(kLogContent));
}

TEST(IsProd, FalseInNonProd) { EXPECT_FALSE(IsProd()); }

}  // namespace
}  // namespace privacy_sandbox::server_common::log
