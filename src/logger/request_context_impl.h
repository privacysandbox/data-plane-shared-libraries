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

#ifndef LOGGER_REQUEST_CONTEXT_IMPL_H_
#define LOGGER_REQUEST_CONTEXT_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/btree_map.h"
#include "absl/functional/any_invocable.h"
#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "opentelemetry/logs/logger_provider.h"
#include "opentelemetry/logs/provider.h"
#include "src/communication/json_utils.h"
#include "src/logger/logger.pb.h"
#include "src/logger/request_context_logger.h"

namespace privacy_sandbox::server_common::log {

inline std::string_view ServerToken(
    std::optional<std::string_view> token = std::nullopt) {
  static std::string* server_token = [token]() {
    if (token == std::nullopt || token->empty()) {
      fprintf(stderr,
              "Warning: server token is not set, consent log is turned off.\n");
      return new std::string("");
    }
    constexpr int kTokenMinLength = 6;
    CHECK(token->length() >= kTokenMinLength)
        << "server token length must be at least " << kTokenMinLength;
    return new std::string(token->data());
  }();
  return *server_token;
}

inline void LogWithPSLog(
    absl::LogSeverity severity,
    privacy_sandbox::server_common::log::PSLogContext& log_context,
    std::string_view msg) {
  switch (severity) {
    case absl::LogSeverity::kInfo:
      PS_LOG(INFO, log_context) << msg;
      break;
    case absl::LogSeverity::kWarning:
      PS_LOG(WARNING, log_context) << msg;
      break;
    default:
      PS_LOG(ERROR, log_context) << msg;
  }
}

ABSL_CONST_INIT inline opentelemetry::logs::Logger* logger_private = nullptr;

// Utility method to format the context provided as key/value pair into a
// string. Function excludes any empty values from the output string.
std::string FormatContext(
    const absl::btree_map<std::string, std::string>& context_map);

opentelemetry::logs::Severity ToOtelSeverity(absl::LogSeverity);

void AddEventMessage(const ::google::protobuf::Message& msg,
                     absl::AnyInvocable<DebugInfo*()>& debug_info);

bool IsProd();

// EventMessageProvider has following interface:
// 1. ::google::protobuf::Message Get()
//  this return the proto message to export
// 2. void Set (const T& field)
//  this sets one of its field
// 3. void ShouldExport ()
//  EventMessageProvider indicates if it should be exported
template <typename EventMessageProvider = std::nullptr_t>
class ContextImpl final : public PSLogContext {
 public:
  ContextImpl(
      const absl::btree_map<std::string, std::string>& context_map,
      const ConsentedDebugConfiguration& debug_config,
      absl::AnyInvocable<DebugInfo*()> debug_info = []() { return nullptr; },
      bool prod_debug = false)
      : consented_sink(ServerToken()),
        debug_response_sink_(std::move(debug_info)) {
    Update(context_map, debug_config, prod_debug);
  }

  // note: base class PSLogContext has no virtual destructor!

  std::string_view ContextStr() const override { return context_; }

  bool is_consented() const override { return consented_sink.is_consented(); }

  absl::LogSink* ConsentedSink() override { return &consented_sink; }

  bool is_debug_response() const override {
    return debug_response_sink_.should_log();
  };

  absl::LogSink* DebugResponseSink() override { return &debug_response_sink_; };

  void Update(const absl::btree_map<std::string, std::string>& new_context,
              const ConsentedDebugConfiguration& debug_config,
              bool prod_debug = false) {
    context_ = FormatContext(new_context);
    consented_sink.client_debug_token_ =
        debug_config.is_consented() ? debug_config.token() : "";
    debug_response_sink_.should_log_ = debug_config.is_debug_info_in_response();
    prod_debug_ = prod_debug;
  }

  template <typename T>
  void SetEventMessageField(T&& field) {
    if constexpr (!std::is_same_v<std::nullptr_t, EventMessageProvider>) {
      if ((is_debug_response() && !IsProd()) || is_consented() || prod_debug_) {
        provider_.Set(std::forward<T>(field));
      }
    }
  }

  void ExportEventMessage(bool if_export_consented = false,
                          bool if_export_prod_debug = false) {
    if constexpr (!std::is_same_v<std::nullptr_t, EventMessageProvider>) {
      if (is_debug_response()) {
        AddEventMessage(provider_.Get(), debug_response_sink_.debug_info_);
      }
      if ((if_export_consented && is_consented()) ||
          (if_export_prod_debug && prod_debug_)) {
        absl::StatusOr<std::string> json_str = ProtoToJson(provider_.Get());
        if (json_str.ok()) {
          logger_private->EmitLogRecord(
              *json_str, opentelemetry::common::MakeAttributes(
                             {{kLogAttribute.data(), "event_message"}}));
        } else {
          logger_private->EmitLogRecord(
              absl::StrCat("ExportEventMessage fail to ProtoToJson:",
                           json_str.status().message()),
              opentelemetry::logs::Severity::kError);
        }
      }
    }
  }

  bool is_prod_debug() const { return prod_debug_; }

  bool ShouldExportEvent() const {
    if constexpr (!std::is_same_v<std::nullptr_t, EventMessageProvider>) {
      return provider_.ShouldExport();
    } else {
      return false;
    }
  }

 private:
  friend class ConsentedLogTest;
  friend class EventMessageTest;

  class ConsentedSinkImpl : public absl::LogSink {
   public:
    explicit ConsentedSinkImpl(std::string_view server_token)
        : server_token_(server_token) {}

    void Send(const absl::LogEntry& entry) override {
      logger_private->EmitLogRecord(
          entry.text_message_with_prefix_and_newline_c_str(),
          ToOtelSeverity(entry.log_severity()));
    }

    void Flush() override {}

    bool is_consented() const {
      return !server_token_.empty() && server_token_ == client_debug_token_;
    }

    // Debug token given by a consented client request.
    std::string client_debug_token_;
    // Debug token owned by the server.
    std::string_view server_token_;
  };

  class DebugResponseSinkImpl : public absl::LogSink {
   public:
    explicit DebugResponseSinkImpl(absl::AnyInvocable<DebugInfo*()> debug_info)
        : debug_info_(std::move(debug_info)) {}

    void Send(const absl::LogEntry& entry) override {
      DebugInfo* debug_info_ptr = debug_info_();
      if (debug_info_ptr != nullptr) {
        debug_info_ptr->add_logs(entry.text_message_with_prefix());
      }
    }

    void Flush() override {}

    bool should_log() const { return should_log_; }

    absl::AnyInvocable<DebugInfo*()> debug_info_;
    bool should_log_ = false;
  };

  void SetServerTokenForTestOnly(std::string_view token) {
    consented_sink.server_token_ = token;
  }

  std::string context_;
  ConsentedSinkImpl consented_sink;
  DebugResponseSinkImpl debug_response_sink_;
  EventMessageProvider provider_;
  bool prod_debug_ = false;

  static constexpr std::string_view kLogAttribute = "ps_tee_log_type";
};

// Defines SafePathContext class to always log to otel for safe code path
class SafePathContext : public PSLogContext {
 public:
  // note: base class PSLogContext has no virtual destructor!
  virtual ~SafePathContext() = default;

  std::string_view ContextStr() const override { return ""; }

  bool is_consented() const override { return true; }

  absl::LogSink* ConsentedSink() override { return &otel_sink_; }

  bool is_debug_response() const override { return false; };
  absl::LogSink* DebugResponseSink() override { return nullptr; };

 protected:
  SafePathContext() = default;
  friend class SafePathLogTest;

 private:
  // OpenTelemetry log sink
  class OtelSinkImpl : public absl::LogSink {
   public:
    OtelSinkImpl() = default;
    void Send(const absl::LogEntry& entry) override {
      logger_private->EmitLogRecord(
          entry.text_message_with_prefix_and_newline_c_str(),
          ToOtelSeverity(entry.log_severity()));
    }
    void Flush() override {}
  };

  OtelSinkImpl otel_sink_;
};

// Log context for system logs that is not on user request path.  `Get()` return
// singleton without construction.
class SystemLogContext : public SafePathContext {
 public:
  static SystemLogContext& Get() {
    static absl::NoDestructor<SystemLogContext> log_instance;
    return *log_instance;
  }

  std::string_view ContextStr() const override {
    return "privacy_sandbox_system_log: ";
  }
};

}  // namespace privacy_sandbox::server_common::log

#endif  // LOGGER_REQUEST_CONTEXT_IMPL_H_
