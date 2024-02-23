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

#ifndef PUBLIC_CPIO_ADAPTERS_PARAMETER_CLIENT_TEST_TEST_AWS_PARAMETER_CLIENT_H_
#define PUBLIC_CPIO_ADAPTERS_PARAMETER_CLIENT_TEST_TEST_AWS_PARAMETER_CLIENT_H_

#include <memory>

#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/adapters/parameter_client/src/parameter_client.h"
#include "src/public/cpio/test/parameter_client/test_aws_parameter_client_options.h"

namespace google::scp::cpio {
/*! @copydoc ParameterClientInterface
 */
class TestAwsParameterClient : public ParameterClient {
 public:
  explicit TestAwsParameterClient(
      const std::shared_ptr<TestAwsParameterClientOptions>& options)
      : ParameterClient(options) {}
};
}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_ADAPTERS_PARAMETER_CLIENT_TEST_TEST_AWS_PARAMETER_CLIENT_H_
