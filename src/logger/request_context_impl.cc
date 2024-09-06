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

#include "src/logger/request_context_impl.h"

#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/strings/str_join.h"

namespace privacy_sandbox::server_common::log {

using OtelSeverity = ::opentelemetry::logs::Severity;

std::string FormatContext(
    const absl::btree_map<std::string, std::string>& context_map) {
  if (context_map.empty()) {
    return "";
  }
  std::vector<std::string> pairs;
  pairs.reserve(context_map.size());
  for (const auto& [key, val] : context_map) {
    if (!val.empty()) {
      pairs.emplace_back(absl::StrCat(key, ": ", val));
    }
  }
  if (pairs.empty()) {
    return "";
  }
  return absl::StrCat(" (", absl::StrJoin(pairs, ", "), ") ");
}

OtelSeverity ToOtelSeverity(absl::LogSeverity severity) {
  static const absl::NoDestructor<
      absl::btree_map<absl::LogSeverity, OtelSeverity>>
      m({
          {absl::LogSeverity::kInfo, OtelSeverity::kInfo},
          {absl::LogSeverity::kWarning, OtelSeverity::kWarn},
          {absl::LogSeverity::kError, OtelSeverity::kError},
          {absl::LogSeverity::kFatal, OtelSeverity::kFatal},
      });
  return m->at(severity);
}

}  // namespace privacy_sandbox::server_common::log
