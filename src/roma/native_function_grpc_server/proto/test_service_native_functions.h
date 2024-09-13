/*
 * Copyright 2024 Google LLC
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

#ifndef PRIVACY_SANDBOX_TEST_SERVICE_NATIVE_FUNCTIONS_H
#define PRIVACY_SANDBOX_TEST_SERVICE_NATIVE_FUNCTIONS_H

#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "src/roma/native_function_grpc_server/proto/multi_service.pb.h"
#include "src/roma/native_function_grpc_server/proto/test_host_service.pb.h"

namespace privacy_sandbox::test_host_server {
template <typename TMetadata>
std::pair<privacy_sandbox::test_host_server::NativeMethodResponse, absl::Status>
HandleNativeMethod(
    const TMetadata& metadata,
    const privacy_sandbox::test_host_server::NativeMethodRequest& request) {
  privacy_sandbox::test_host_server::NativeMethodResponse response;
  response.set_output(
      absl::StrCat(request.input(), "World. From NativeMethod"));
  return std::make_pair(response, absl::OkStatus());
}
}  // namespace privacy_sandbox::test_host_server

namespace privacy_sandbox::multi_service {
template <typename TMetadata>
std::pair<privacy_sandbox::multi_service::TestMethod1Response, absl::Status>
HandleTestMethod1(
    const TMetadata& metadata,
    const privacy_sandbox::multi_service::TestMethod1Request& request) {
  privacy_sandbox::multi_service::TestMethod1Response response;
  response.set_output(absl::StrCat(request.input(), "World. From TestMethod1"));
  return std::make_pair(response, absl::OkStatus());
}

template <typename TMetadata>
std::pair<privacy_sandbox::multi_service::TestMethod2Response, absl::Status>
HandleTestMethod2(
    const TMetadata& metadata,
    const privacy_sandbox::multi_service::TestMethod2Request& request) {
  privacy_sandbox::multi_service::TestMethod2Response response;
  response.set_output(absl::StrCat(request.input(), "World. From TestMethod2"));
  return std::make_pair(response, absl::OkStatus());
}
}  // namespace privacy_sandbox::multi_service

#endif  // PRIVACY_SANDBOX_TEST_SERVICE_NATIVE_FUNCTIONS_H
