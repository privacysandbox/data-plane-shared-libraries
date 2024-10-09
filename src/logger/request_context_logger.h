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

#ifndef LOGGER_REQUEST_CONTEXT_LOGGER_H_
#define LOGGER_REQUEST_CONTEXT_LOGGER_H_

#include <optional>

#include "absl/log/absl_log.h"
#include "absl/log/log_sink.h"

namespace privacy_sandbox::server_common::log {

// Updates global verbosity level for PS_VLOG
void SetGlobalPSVLogLevel(int verbosity_level);

// Reads global PS_VLOG verbosity level and determines whether to
// log for a given verbosity or not
bool PS_VLOG_IS_ON(int verbosity_level);

// Set to true to always log to OTel in non_prod. Calling first time sets
// `always_log_otel`, after first time `log_otel` is ignored.
inline bool AlwaysLogOtel(std::optional<bool> log_otel = std::nullopt) {
  static bool always_log_otel = [log_otel]() {
    if (log_otel == std::nullopt) {
      fprintf(stderr,
              "Info: always_log_otel is not set, only consented is logged to "
              "OTel.\n");
      return false;
    } else {
      return *log_otel;
    }
  }();
  return always_log_otel;
}

// Used by `PS_VLOG`, to provide the context of how to log a request.
class PSLogContext {
 protected:
  ~PSLogContext() = default;

 public:
  // `ContextStr()` will be added to the front of log message.
  virtual std::string_view ContextStr() const = 0;
  // if `is_consented()`, `ConsentedSink()` will log
  virtual bool is_consented() const = 0;
  virtual absl::LogSink* ConsentedSink() = 0;
  // if `is_debug_response()`, `DebugResponseSink()` will log
  virtual bool is_debug_response() const = 0;
  virtual absl::LogSink* DebugResponseSink() = 0;
};

class NoOpContext final : public PSLogContext {
 public:
  // note: base class PSLogContext has no virtual destructor!

  std::string_view ContextStr() const override { return ""; }

  bool is_consented() const override { return false; }

  absl::LogSink* ConsentedSink() override { return nullptr; }

  bool is_debug_response() const override { return false; }

  absl::LogSink* DebugResponseSink() override { return nullptr; }
};

inline constexpr NoOpContext kNoOpContext;

// Extend LogMessage to be able to conditionally add LogSink
class PSLogMessage : public absl::log_internal::LogMessage {
 public:
  using LogMessage::LogMessage;

  PSLogMessage& ToSinkAlsoIf(bool condition, absl::LogSink* sink) {
    if (condition) {
      ToSinkAlso(sink);
    }
    return *this;
  }
};

}  // namespace privacy_sandbox::server_common::log

#ifdef PS_LOG_NON_PROD
#define PS_VLOG_INTERNAL(verbose_level, ps_log_context)                     \
  switch (::privacy_sandbox::server_common::log::                           \
              PSLogContext& ps_logging_internal_context = (ps_log_context); \
          const int ps_logging_internal_verbose_level = (verbose_level))    \
  default:                                                                  \
    PS_LOG_INTERNAL_LOG_IF_IMPL(                                            \
        _INFO,                                                              \
        ::privacy_sandbox::server_common::log::PS_VLOG_IS_ON(               \
            ps_logging_internal_verbose_level),                             \
        ps_logging_internal_context)                                        \
        .WithVerbosity(ps_logging_internal_verbose_level)

#define PS_LOG_INTERNAL(severity, ps_log_context)                           \
  switch (::privacy_sandbox::server_common::log::                           \
              PSLogContext& ps_logging_internal_context = (ps_log_context); \
          0)                                                                \
  default:                                                                  \
    PS_LOG_INTERNAL_LOG_IF_IMPL(_##severity, true, ps_logging_internal_context)

#else
#define PS_VLOG_INTERNAL(verbose_level, ps_log_context)                     \
  switch (::privacy_sandbox::server_common::log::                           \
              PSLogContext& ps_logging_internal_context = (ps_log_context); \
          const int ps_logging_internal_verbose_level = (verbose_level))    \
  default:                                                                  \
    ABSL_LOG_IF(INFO, ::privacy_sandbox::server_common::log::PS_VLOG_IS_ON( \
                          ps_logging_internal_verbose_level) &&             \
                          ps_logging_internal_context.is_consented())       \
        .ToSinkOnly(ps_logging_internal_context.ConsentedSink())

#define PS_LOG_INTERNAL(severity, ps_log_context)                           \
  switch (::privacy_sandbox::server_common::log::                           \
              PSLogContext& ps_logging_internal_context = (ps_log_context); \
          0)                                                                \
  default:                                                                  \
    ABSL_LOG_IF(severity, ps_logging_internal_context.is_consented())       \
        .ToSinkOnly(ps_logging_internal_context.ConsentedSink())

#endif

#define PS_LOG_INTERNAL_LOG_IF_IMPL(severity, condition, ps_log_context) \
  PS_LOG_INTERNAL_CONDITION(condition)                                   \
  PS_LOGGING_INTERNAL_LOG##severity(ps_log_context).InternalStream()

// The `switch` ensures that this expansion is the beginning of a statement.
//
// The tenary evaluates to either
//   (void)0;
// or
//   ::absl::log_internal::Voidify() &&
//       PS_LOGGING_INTERNAL_LOG_INFO(ps_log_context) << "log message";
//
// `Voidify()` is to avoid compiler wanring.
#define PS_LOG_INTERNAL_CONDITION(condition) \
  switch (0)                                 \
  case 0:                                    \
  default:                                   \
    !(condition) ? (void)0 : ::absl::log_internal::Voidify()&&

// This must only be used in non_prod.
#define PS_LOGGING_INTERNAL_LOG_SEVERITY(ps_log_context, absl_log_severity) \
  ::privacy_sandbox::server_common::log::PSLogMessage(__FILE__, __LINE__,   \
                                                      absl_log_severity)    \
      .ToSinkAlsoIf(                                                        \
          (ps_log_context).is_consented() ||                                \
              ::privacy_sandbox::server_common::log::AlwaysLogOtel() &&     \
                  (ps_log_context).ConsentedSink() != nullptr,              \
          (ps_log_context).ConsentedSink())                                 \
      .ToSinkAlsoIf((ps_log_context).is_debug_response(),                   \
                    (ps_log_context).DebugResponseSink())

#define PS_LOGGING_INTERNAL_LOG_INFO(ps_log_context) \
  PS_LOGGING_INTERNAL_LOG_SEVERITY(ps_log_context, ::absl::LogSeverity::kInfo)

#define PS_LOGGING_INTERNAL_LOG_WARNING(ps_log_context) \
  PS_LOGGING_INTERNAL_LOG_SEVERITY(ps_log_context,      \
                                   ::absl::LogSeverity::kWarning)

#define PS_LOGGING_INTERNAL_LOG_ERROR(ps_log_context) \
  PS_LOGGING_INTERNAL_LOG_SEVERITY(ps_log_context, ::absl::LogSeverity::kError)

#define PS_VLOG_CONTEXT_INTERNAL(verbose_level, ps_log_context) \
  PS_VLOG_INTERNAL(verbose_level, ps_log_context)               \
      << ps_logging_internal_context.ContextStr()

#define PS_LOG_CONTEXT_INTERNAL(severity, ps_log_context) \
  PS_LOG_INTERNAL(severity, ps_log_context)               \
      << ps_logging_internal_context.ContextStr()

#define PS_VLOG_NO_CONTEXT_INTERNAL(verbose_level)                     \
  PS_VLOG_INTERNAL(                                                    \
      verbose_level,                                                   \
      const_cast<::privacy_sandbox::server_common::log::NoOpContext&>( \
          ::privacy_sandbox::server_common::log::kNoOpContext))

#define PS_LOG_NO_CONTEXT_INTERNAL(severity)                           \
  PS_LOG_INTERNAL(                                                     \
      severity,                                                        \
      const_cast<::privacy_sandbox::server_common::log::NoOpContext&>( \
          ::privacy_sandbox::server_common::log::kNoOpContext))

// It can have 1 or 2 arguments. i.e.
// PS_VLOG(verbose_level)
//   Same as ABSL_VLOG(verbose_level), but only log in `non_prod`
// PS_VLOG(verbose_level, ps_log_context)
//   Similar to ABSL_VLOG(verbose_level), but with extra functionality using
//   `ps_log_context` (as `PSLogContext&`)
#define PS_VLOG(...)                                 \
  GET_PS_VLOG(__VA_ARGS__, PS_VLOG_CONTEXT_INTERNAL, \
              PS_VLOG_NO_CONTEXT_INTERNAL)           \
  (__VA_ARGS__)
#define GET_PS_VLOG(_1, _2, NAME, ...) NAME

#define PS_LOG(...)                                                            \
  GET_PS_LOG(__VA_ARGS__, PS_LOG_CONTEXT_INTERNAL, PS_LOG_NO_CONTEXT_INTERNAL) \
  (__VA_ARGS__)
#define GET_PS_LOG(_1, _2, NAME, ...) NAME

#endif  // LOGGER_REQUEST_CONTEXT_LOGGER_H_
