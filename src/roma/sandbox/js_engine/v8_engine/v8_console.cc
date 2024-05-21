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

#include "v8_console.h"

#include <stdio.h>

#include <fstream>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/log.h"
#include "absl/strings/str_join.h"
#include "include/v8.h"

constexpr std::string_view kCouldNotConvertArgToString =
    "V8_CONSOLE: Could not perform string conversion for argument ";

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

namespace {
std::vector<std::string> GetLogMsg(
    v8::Isolate* isolate, const v8::debug::ConsoleCallArguments& args) {
  std::vector<std::string> msgs;
  for (int i = 0; i < args.Length(); i++) {
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Value> arg = args[i];
    v8::Local<v8::String> str_obj;

    if (arg->IsSymbol()) {
      arg = v8::Local<v8::Symbol>::Cast(arg)->Description(isolate);
    }
    if (!arg->ToString(isolate->GetCurrentContext()).ToLocal(&str_obj)) {
      ROMA_VLOG(1) << kCouldNotConvertArgToString << i;
      continue;
    }

    v8::String::Utf8Value str(isolate, str_obj);
    msgs.emplace_back(*str);
  }
  return msgs;
}
}  // anonymous namespace

V8Console::V8Console(v8::Isolate* isolate, LogFunctionHandler handle_log_func)
    : isolate_(isolate), handle_log_func_(std::move(handle_log_func)) {}

void V8Console::Log(const v8::debug::ConsoleCallArguments& args,
                    const v8::debug::ConsoleContext&) {
  HandleLog(args, "ROMA_LOG");
}

void V8Console::Warn(const v8::debug::ConsoleCallArguments& args,
                     const v8::debug::ConsoleContext&) {
  HandleLog(args, "ROMA_WARN");
}

void V8Console::Error(const v8::debug::ConsoleCallArguments& args,
                      const v8::debug::ConsoleContext&) {
  HandleLog(args, "ROMA_ERROR");
}

void V8Console::SetIds(std::string_view uuid, std::string_view id) {
  invocation_req_uuid_ = uuid;
  invocation_req_id_ = id;
}

void V8Console::SetMinLogLevel(absl::LogSeverity severity) {
  min_log_level_ = severity;
}

void V8Console::HandleLog(const v8::debug::ConsoleCallArguments& args,
                          std::string_view function_name) {
  const auto msgs = GetLogMsg(isolate_, args);
  const std::string msg = absl::StrJoin(msgs, " ");
  handle_log_func_(function_name, msg,
                   {
                       .uuid = invocation_req_uuid_,
                       .id = invocation_req_id_,
                       .min_log_level = min_log_level_,
                   });
}

}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine
