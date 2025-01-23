/*
 * Copyright 2025 Google LLC
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

#ifndef SRC_ROMA_BYOB_BENCHMARK_LATENCY_FORMATTER_H_
#define SRC_ROMA_BYOB_BENCHMARK_LATENCY_FORMATTER_H_

#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/time/time.h"
#include "src/roma/interface/roma.h"

namespace privacy_sandbox::server_common::byob {

class LatencyFormatter {
 public:
  // Formats a batch of responses into a pipe-delimited string
  static std::string Stringify(
      const std::vector<absl::StatusOr<google::scp::roma::ResponseObject>>&
          batch_resp,
      std::string delimiter = "|") {
    return absl::StrJoin(batch_resp, delimiter, ResponseFormatter());
  }

  // Parses a pipe-delimited string of durations into a vector of durations
  static std::vector<absl::Duration> Parse(
      const std::vector<absl::StatusOr<std::string>>& invocation_outputs,
      std::string delimiter = "|") {
    std::vector<absl::Duration> output_latencies;
    for (const auto& output : invocation_outputs) {
      if (output.ok()) {
        auto toks = absl::StrSplit(*output, delimiter);
        bool duration_added = false;
        for (const auto& tok : toks) {
          absl::Duration duration;
          if (absl::ParseDuration(tok, &duration)) {
            output_latencies.push_back(duration);
            duration_added = true;
          } else {
            break;
          }
        }
        // Invocation succeeded, but the output was not a duration, or a |
        // delimited list of durations
        if (!duration_added) {
          break;
        }
      }
    }
    return output_latencies;
  }

 private:
  struct ResponseFormatter {
    void operator()(
        std::string* out,
        const absl::StatusOr<google::scp::roma::ResponseObject>& resp) const {
      if (resp.ok()) {
        out->append(resp->resp.substr(1, resp->resp.size() - 2));
      } else {
        LOG(ERROR) << "Could not append response: " << resp.status();
      }
    }
  };
};

}  // namespace privacy_sandbox::server_common::byob

#endif  // SRC_ROMA_BYOB_BENCHMARK_LATENCY_FORMATTER_H_
