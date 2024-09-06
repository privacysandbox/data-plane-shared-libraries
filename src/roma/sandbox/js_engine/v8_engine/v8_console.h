/*
 * Copyright 2023 Google LLC
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
#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_CONSOLE_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_CONSOLE_H_
#undef ABSL_LOG_CHECK_H_

// Clang is disabled in the include statements below because including
// src/debug/interface-types.h (needed for v8::debug::ConsoleDelegate) causes a
// macro collision with the existing definition of CHECK from absl/log/check.h.
// To fix the macro collision, ABSL_LOG_CHECK_H_ is undefined on line 28,
// src/debug/interface-types.h is included in line 32, and absl/log/check.h is
// included in line 33 to overwrite the CHECK definition from interface-types.h.
// Clang needs to be disabled because interface-types.h can't be included after
// check.h, as CHECK needs to be first defined by interface-types.h
// clang-format off

#include <string>

#include "src/debug/interface-types.h"
#include "absl/base/nullability.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/functional/any_invocable.h"
#include "src/roma/sandbox/native_function_binding/rpc_wrapper.pb.h"
#include "src/roma/logging/logging.h"

// clang-format on

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

class V8Console : public v8::debug::ConsoleDelegate {
 private:
  using LogFunctionHandler =
      absl::AnyInvocable<absl::Status(std::string_view, std::string_view,
                                      google::scp::roma::logging::LogOptions)>;

 public:
  explicit V8Console(absl::Nonnull<v8::Isolate*> isolate,
                     LogFunctionHandler handle_log_func_);
  ~V8Console() override = default;

  void SetIds(std::string_view uuid, std::string_view id);
  void SetMinLogLevel(absl::LogSeverity severity);

 private:
  void Log(const v8::debug::ConsoleCallArguments& args,
           const v8::debug::ConsoleContext&) override;
  void Warn(const v8::debug::ConsoleCallArguments& args,
            const v8::debug::ConsoleContext&) override;
  void Error(const v8::debug::ConsoleCallArguments& args,
             const v8::debug::ConsoleContext&) override;

  void HandleLog(const v8::debug::ConsoleCallArguments& args,
                 std::string_view function_name);

  v8::Isolate* isolate_;
  std::string invocation_req_uuid_;
  std::string invocation_req_id_;
  absl::LogSeverity min_log_level_ = absl::LogSeverity::kInfo;
  LogFunctionHandler handle_log_func_;
};

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_V8_CONSOLE_H_
