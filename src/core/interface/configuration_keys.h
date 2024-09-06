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

#ifndef CORE_INTERFACE_CONFIGURATION_KEYS_H_
#define CORE_INTERFACE_CONFIGURATION_KEYS_H_

#include <string_view>

namespace google::scp::core {
// AWS/GCP cloud region name
inline constexpr std::string_view kCloudServiceRegion =
    "google_scp_core_cloud_region";
// GCP Project Id (Not project name)
inline constexpr std::string_view kGcpProjectId = "google_scp_gcp_project_id";
// Skip a log if unable to apply during log recovery
inline constexpr std::string_view kAggregatedMetricIntervalMs =
    "google_scp_aggregated_metric_interval_ms";
inline constexpr std::string_view kHTTPServerRequestRoutingEnabled =
    "google_scp_http_server_request_routing_enabled";
}  // namespace google::scp::core

#endif  // CORE_INTERFACE_CONFIGURATION_KEYS_H_
