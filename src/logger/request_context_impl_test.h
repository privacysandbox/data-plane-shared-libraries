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
#ifndef LOGGER_REQUEST_CONTEXT_IMPL_TEST_H_
#define LOGGER_REQUEST_CONTEXT_IMPL_TEST_H_

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <utility>

#include "absl/log/check.h"
#include "opentelemetry/exporters/ostream/log_record_exporter.h"
#include "opentelemetry/sdk/logs/logger_provider_factory.h"
#include "opentelemetry/sdk/logs/simple_log_record_processor_factory.h"
#include "src/logger/request_context_impl.h"
#include "src/logger/request_context_logger_test.h"
#include "src/util/protoutil.h"

namespace privacy_sandbox::server_common::log {

namespace logs_api = opentelemetry::logs;
namespace logs_sdk = opentelemetry::sdk::logs;
namespace logs_exporter = opentelemetry::exporter::logs;

class ConsentedLogTest : public test::LogTest {
 protected:
  void SetUp() override {
    // initialize max verbosity = kMaxV
    SetGlobalPSVLogLevel(kMaxV);

    logger_ = logs_sdk::LoggerProviderFactory::Create(
        logs_sdk::SimpleLogRecordProcessorFactory::Create(
            std::make_unique<logs_exporter::OStreamLogRecordExporter>(
                GetSs())));
    logger_private = logger_->GetLogger("default").get();

    mismatched_token_ = ParseTextOrDie<ConsentedDebugConfiguration>(R"pb(
      is_consented: true
      token: "mismatched_Token"
    )pb");

    matched_token_ = ParseTextOrDie<ConsentedDebugConfiguration>(R"pb(
      is_consented: true
      token: "server_tok"
    )pb");
  }

  static std::stringstream& GetSs() {
    // never destructed, outlive 'OStreamLogRecordExporter'
    static auto* ss = new std::stringstream();
    return *ss;
  }

  std::string ReadSs() {
    // Shut down reader now to avoid concurrent access of Ss.
    logger_.reset();
    test_instance_.reset();
    std::string output = GetSs().str();
    GetSs().str("");
    return output;
  }

  void SetServerTokenForTestOnly(std::string_view token) {
    test_instance_->SetServerTokenForTestOnly(token);
  }

  std::unique_ptr<logs_api::LoggerProvider> logger_;
  std::unique_ptr<ContextImpl<>> test_instance_;
  ConsentedDebugConfiguration matched_token_, mismatched_token_;

  const std::string_view kServerToken = "server_tok";
};

class DebugResponseTest : public ConsentedLogTest {
 protected:
  void SetUp() override {
    ConsentedLogTest::SetUp();
    debug_info_config_.set_is_debug_info_in_response(true);
    matched_token_debug_info_set_true_ = matched_token_;
    matched_token_debug_info_set_true_.set_is_debug_info_in_response(true);
  }

  bool accessed_debug_info_ = false;
  DebugInfo debug_info_;
  ConsentedDebugConfiguration debug_info_config_,
      matched_token_debug_info_set_true_;
};

class MockEventMessageProvider {
 public:
  const ::google::protobuf::Message& Get() { return event_message_; }

  void Set(const std::string& field) {
    event_message_.set_generation_id(field);
  }

 private:
  LogContext event_message_;
};

class EventMessageTest : public ConsentedLogTest {
 protected:
  std::string ReadSs() {
    // Shut down reader now to avoid concurrent access of Ss.
    logger_.reset();
    em_instance_.reset();
    std::string output = GetSs().str();
    GetSs().str("");
    return output;
  }

  void SetServerTokenForTestOnly(std::string_view token) {
    em_instance_->SetServerTokenForTestOnly(token);
  }

  std::unique_ptr<ContextImpl<MockEventMessageProvider>> em_instance_;
};

class SafePathLogTest : public ConsentedLogTest {
 protected:
  static std::unique_ptr<SafePathContext> CreateTestInstance() {
    return std::unique_ptr<SafePathContext>(new SafePathContext());
  }
  void SetUp() override { ConsentedLogTest::SetUp(); }
  std::unique_ptr<SafePathContext> test_instance_;
};

class SystemLogTest : public ConsentedLogTest {};

}  // namespace privacy_sandbox::server_common::log
#endif  // LOGGER_REQUEST_CONTEXT_IMPL_TEST_H_
