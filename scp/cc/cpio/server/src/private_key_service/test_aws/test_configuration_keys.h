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
// Optional. Only needed for AWS. If not set, use the default value us-east-1.
static constexpr char kTestAwsPrivateKeyClientRegion[] =
    "cmrt_sdk_test_aws_private_key_client_region";
// Optional. Only needed for AWS integration test.
static constexpr char kTestAwsPrivateKeyClientKmsEndpointOverride[] =
    "cmrt_sdk_test_aws_private_key_client_kms_endpoint_override";
// Optional. Only needed for AWS integration test.
static constexpr char kTestAwsPrivateKeyClientStsEndpointOverride[] =
    "cmrt_sdk_test_aws_private_key_client_sts_endpoint_override";
// Optional bool. If not set, use the default value false.
static constexpr char kTestAwsPrivateKeyClientIntegrationTest[] =
    "cmrt_sdk_test_aws_private_key_client_integration_test";
}  // namespace google::scp::cpio
