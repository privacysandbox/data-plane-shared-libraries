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

#ifndef CPIO_SERVER_SRC_INSTANCE_SERVICE_TEST_GCP_CONFIGURATION_KEYS_H_
#define CPIO_SERVER_SRC_INSTANCE_SERVICE_TEST_GCP_CONFIGURATION_KEYS_H_

namespace google::scp::cpio {

// Required. It should be a real project for your service account which is used
// for your test.
static constexpr char kTestGcpInstanceClientProjectId[] =
    "cmrt_sdk_test_gcp_instance_client_project_id";
// Optional. It should be a real zone for your service account which is used for
// your test. If not set, use the default value us-central1-a.
static constexpr char kTestGcpInstanceClientZone[] =
    "cmrt_sdk_test_gcp_instance_client_zone";
// Optional. It should be a real instance id for your service account which is
// used for your test. If not set, use the default value 12345678987654321.
// Please note this is not a real instance and is only a dummy for testing
// server without using real instance.
static constexpr char kTestGcpInstanceClientInstanceId[] =
    "cmrt_sdk_test_gcp_instance_client_instance_id";

}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_SRC_INSTANCE_SERVICE_TEST_GCP_CONFIGURATION_KEYS_H_
