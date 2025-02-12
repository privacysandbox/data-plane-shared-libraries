/*
 * Copyright 2022 Google LLC
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

#ifndef CORE_INTERFACE_TYPE_DEF_H_
#define CORE_INTERFACE_TYPE_DEF_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "src/core/common/proto/common.pb.h"

namespace google::scp::core {
using Timestamp = uint64_t;
using TimeDuration = uint64_t;
using Byte = char;
using Token = std::string;

/// The default aggregate interval in milliseconds for AggregatedMetric.
inline constexpr TimeDuration kDefaultAggregatedMetricIntervalMs = 1000;

using PublicPrivateKeyPairId = std::string;

/// Struct that stores version metadata.
struct Version {
  uint64_t major = 0;
  uint64_t minor = 0;

  bool operator==(const Version& other) const {
    return major == other.major && minor == other.minor;
  }
};

// The http header for the client activity id
inline constexpr std::string_view kClientActivityIdHeader =
    "x-gscp-client-activity-id";
inline constexpr std::string_view kClaimedIdentityHeader =
    "x-gscp-claimed-identity";
inline constexpr std::string_view kAuthHeader = "x-auth-token";

struct LoadableObject {
  LoadableObject() : is_loaded(false), needs_loader(false) {}

  virtual ~LoadableObject() = default;

  std::atomic<bool> is_loaded;
  std::atomic<bool> needs_loader;
};

inline constexpr TimeDuration kAsyncContextExpirationDurationInSeconds = 90;

// The default config value for RetryStrategyOptions
inline constexpr size_t kDefaultRetryStrategyMaxRetries = 12;
inline constexpr TimeDuration kDefaultRetryStrategyDelayInMs = 101;

// The default config value for HttpClientOptions
inline constexpr size_t kDefaultMaxConnectionsPerHost = 2;
inline constexpr TimeDuration kDefaultHttp2ReadTimeoutInSeconds = 60;

}  // namespace google::scp::core

#endif  // CORE_INTERFACE_TYPE_DEF_H_
