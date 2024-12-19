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
#include "src/logger/request_context_impl_test.h"

namespace privacy_sandbox::server_common::log {
namespace {

using ::testing::ContainsRegex;
using ::testing::IsEmpty;
using ::testing::StrEq;

TEST_F(ConsentedLogTest, LogNotConsented) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      mismatched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_VLOG(kMaxV, *test_instance_) << kLogContent; }),
              IsEmpty());
  EXPECT_THAT(ReadSs(), IsEmpty());
}

TEST_F(ConsentedLogTest, LogConsented) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_VLOG(kMaxV, *test_instance_) << kLogContent; }),
              IsEmpty());
  std::string log_str = ReadSs();
  EXPECT_THAT(log_str,
              ContainsRegex(absl::StrCat("\\(id: 1234\\)[ \t]+", kLogContent)));

  EXPECT_THAT(log_str, ContainsRegex("severity_text[ \t:]+INFO"));
}

TEST_F(ConsentedLogTest, LogSeverityWarn) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);

  EXPECT_THAT(LogWithCapturedStderr([this]() {
                PS_LOG(WARNING, *test_instance_) << kLogContent;
              }),
              IsEmpty());
  EXPECT_THAT(ReadSs(), ContainsRegex("severity_text[ \t:]+WARN"));
}

TEST_F(ConsentedLogTest, LogSeverityError) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);

  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_LOG(ERROR, *test_instance_) << kLogContent; }),
              IsEmpty());
  EXPECT_THAT(ReadSs(), ContainsRegex("severity_text[ \t:]+ERROR"));
}

TEST_F(DebugResponseTest, NotLoggedInProd) {
  // mismatched_token_ doesn't log
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      mismatched_token_, [this]() {
        accessed_debug_info_ = true;
        return &debug_info_;
      });
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_FALSE(accessed_debug_info_);

  // matched_token_ doesn't log
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}}, matched_token_,
      [this]() {
        accessed_debug_info_ = true;
        return &debug_info_;
      });
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_THAT(ReadSs(),
              ContainsRegex(absl::StrCat("\\(id: 1234\\)[ \t]+", kLogContent)));
  EXPECT_FALSE(accessed_debug_info_);

  // debug_info turned on, but doesn't log
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      debug_info_config_, [this]() {
        accessed_debug_info_ = true;
        return &debug_info_;
      });
  SetServerTokenForTestOnly(kServerToken);
  PS_VLOG(kMaxV, *test_instance_) << kLogContent;
  EXPECT_FALSE(accessed_debug_info_);
}

TEST_F(DebugResponseTest, NoDebugInfoProvided) {
  // No DebugInfo is provided but debug_info_config set to true and
  // matched token is passed
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{{"id", "1234"}},
      matched_token_debug_info_set_true_);
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

TEST_F(DebugResponseTest, EventMessage) {
  // debug_info turned on, but doesn't store EventMessage
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
}

TEST_F(EventMessageTest, Consented) {
  em_instance_ = std::make_unique<ContextImpl<MockEventMessageProvider>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  CHECK(em_instance_->is_consented());
  em_instance_->SetEventMessageField(std::string("test gen id"));
  em_instance_->ExportEventMessage(/*if_export_consented=*/true);
  std::string otlp_output = ReadSs();
  EXPECT_THAT(otlp_output, ContainsRegex("test gen id"));
  EXPECT_THAT(otlp_output, ContainsRegex("ps_tee_log_type: event_message"));
}

TEST_F(EventMessageTest, NotConsented) {
  em_instance_ = std::make_unique<ContextImpl<MockEventMessageProvider>>(
      absl::btree_map<std::string, std::string>{}, mismatched_token_);
  SetServerTokenForTestOnly(kServerToken);
  CHECK(!em_instance_->is_consented());
  em_instance_->SetEventMessageField(std::string("test gen id"));
  em_instance_->ExportEventMessage(/*if_export_consented=*/true);
  EXPECT_THAT(ReadSs(), IsEmpty());
}

TEST_F(EventMessageTest, ProdDebug) {
  em_instance_ = std::make_unique<ContextImpl<MockEventMessageProvider>>(
      absl::btree_map<std::string, std::string>{}, mismatched_token_,
      []() { return nullptr; }, /*prod_debug =*/true);
  SetServerTokenForTestOnly(kServerToken);
  CHECK(!em_instance_->is_consented());
  em_instance_->SetEventMessageField(std::string("test gen id"));
  em_instance_->ExportEventMessage(/*if_export_consented=*/true,
                                   /*if_export_prod_debug=*/true);
  std::string otlp_output = ReadSs();
  EXPECT_THAT(otlp_output, ContainsRegex("test gen id"));
  EXPECT_THAT(otlp_output, ContainsRegex("ps_tee_log_type: event_message"));
}

TEST(FormatContext, NoContextGeneratesEmptyString) {
  EXPECT_THAT(FormatContext({}), IsEmpty());
}

TEST(FormatContext, SingleKeyValFormatting) {
  EXPECT_THAT(FormatContext({{"key1", "val1"}}), StrEq(" (key1: val1) "));
}

TEST(FormatContext, MultipleKeysLexicographicallyOrdered) {
  EXPECT_THAT(FormatContext({{"key1", "val1"}, {"key2", "val2"}}),
              StrEq(" (key1: val1, key2: val2) "));
}

TEST(FormatContext, OptionalValuesNotInTheFormattedOutput) {
  EXPECT_THAT(FormatContext({{"key1", ""}}), IsEmpty());
}

TEST_F(ConsentedLogTest, NotConsented) {
  // default
  EXPECT_FALSE(ContextImpl({}, ConsentedDebugConfiguration()).is_consented());

  // no client token
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{},
      ConsentedDebugConfiguration());
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_FALSE(test_instance_->is_consented());

  // empty server and client token
  auto empty_client_token = ParseTextOrDie<ConsentedDebugConfiguration>(R"pb(
    is_consented: true
    token: ""
  )pb");
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, empty_client_token);
  SetServerTokenForTestOnly("");
  EXPECT_FALSE(test_instance_->is_consented());

  // empty client token, valid server token
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, empty_client_token);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_FALSE(test_instance_->is_consented());

  // valid client token, empty server token
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly("");
  EXPECT_FALSE(test_instance_->is_consented());

  // mismatch
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, mismatched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_FALSE(test_instance_->is_consented());
}

TEST_F(ConsentedLogTest, ConsentRevocation) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_TRUE(test_instance_->is_consented());

  matched_token_.set_is_consented(false);
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_FALSE(test_instance_->is_consented());
}

TEST_F(ConsentedLogTest, Update) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);
  EXPECT_TRUE(test_instance_->is_consented());
  test_instance_->Update({}, ConsentedDebugConfiguration());
  EXPECT_FALSE(test_instance_->is_consented());
  test_instance_->Update({}, matched_token_);
  EXPECT_TRUE(test_instance_->is_consented());
  test_instance_->Update({}, mismatched_token_);
  EXPECT_FALSE(test_instance_->is_consented());
}

TEST_F(ConsentedLogTest, LogWithDynamicSeverities) {
  test_instance_ = std::make_unique<ContextImpl<>>(
      absl::btree_map<std::string, std::string>{}, matched_token_);
  SetServerTokenForTestOnly(kServerToken);

  EXPECT_THAT(LogWithCapturedStderr([this]() {
                LogWithPSLog(absl::LogSeverity::kError, *test_instance_,
                             kLogContent);
              }),
              IsEmpty());
  EXPECT_THAT(GetSs().str(), ContainsRegex("severity_text[ \t:]+ERROR"));
  EXPECT_THAT(LogWithCapturedStderr([this]() {
                LogWithPSLog(absl::LogSeverity::kFatal, *test_instance_,
                             kLogContent);
              }),
              IsEmpty());
  EXPECT_THAT(GetSs().str(), ContainsRegex("severity_text[ \t:]+ERROR"));
  EXPECT_THAT(LogWithCapturedStderr([this]() {
                LogWithPSLog(absl::LogSeverity::kInfo, *test_instance_,
                             kLogContent);
              }),
              IsEmpty());
  EXPECT_THAT(GetSs().str(), ContainsRegex("severity_text[ \t:]+INFO"));
  EXPECT_THAT(LogWithCapturedStderr([this]() {
                LogWithPSLog(absl::LogSeverity::kWarning, *test_instance_,
                             kLogContent);
              }),
              IsEmpty());
  EXPECT_THAT(GetSs().str(), ContainsRegex("severity_text[ \t:]+WARN"));
}

TEST_F(SafePathLogTest, LogMessage) {
  test_instance_ = CreateTestInstance();
  EXPECT_THAT(LogWithCapturedStderr(
                  [this]() { PS_VLOG(kMaxV, *test_instance_) << kLogContent; }),
              IsEmpty());
  EXPECT_THAT(ReadSs(), ContainsRegex(kLogContent));
}

TEST_F(SystemLogTest, LogMessage) {
  EXPECT_THAT(LogWithCapturedStderr([]() {
                PS_VLOG(kMaxV, SystemLogContext::Get()) << kLogContent;
              }),
              IsEmpty());
  EXPECT_THAT(ReadSs(), ContainsRegex(kLogContent));
}

TEST(IsProd, TrueInProd) { EXPECT_TRUE(IsProd()); }

}  // namespace
}  // namespace privacy_sandbox::server_common::log
