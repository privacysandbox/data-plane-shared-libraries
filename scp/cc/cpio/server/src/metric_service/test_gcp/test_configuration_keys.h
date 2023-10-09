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

#pragma once

namespace google::scp::cpio {
// Required. Only needed for GCP. It should be a real project for your service
// account which is used for your test.
static constexpr char kTestGcpMetricClientProjectId[] =
    "cmrt_sdk_test_gcp_metric_client_project_id";
// Optional. Only needed for GCP. If not set, use the default value
// us-central1-c.
static constexpr char kTestGcpMetricClientZoneId[] =
    "cmrt_sdk_test_gcp_metric_client_zone_id";
// Required. Only needed for GCP. It should be a real instance ID.
static constexpr char kTestGcpMetricClientInstanceId[] =
    "cmrt_sdk_test_gcp_metric_client_instance_id";
}  // namespace google::scp::cpio
