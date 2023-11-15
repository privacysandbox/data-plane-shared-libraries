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

#ifndef CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_MOCK_AWS_MOCK_SSM_CLIENT_H_
#define CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_MOCK_AWS_MOCK_SSM_CLIENT_H_

#include <memory>

#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/GetParameterRequest.h>

namespace google::scp::cpio::client_providers::mock {
class MockSSMClient : public Aws::SSM::SSMClient {
 public:
  void GetParameterAsync(
      const Aws::SSM::Model::GetParameterRequest& request,
      const Aws::SSM::GetParameterResponseReceivedHandler& handler,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>& context =
          nullptr) const override {
    if (request.GetName() == get_parameter_request_mock.GetName()) {
      handler(this, request, get_parameter_outcome_mock, context);
      return;
    }
    Aws::SSM::Model::GetParameterResult result;
    handler(this, request, Aws::SSM::Model::GetParameterOutcome(result),
            context);
  }

  Aws::SSM::Model::GetParameterRequest get_parameter_request_mock;
  Aws::SSM::Model::GetParameterOutcome get_parameter_outcome_mock;
};
}  // namespace google::scp::cpio::client_providers::mock

#endif  // CPIO_CLIENT_PROVIDERS_PARAMETER_CLIENT_PROVIDER_MOCK_AWS_MOCK_SSM_CLIENT_H_
