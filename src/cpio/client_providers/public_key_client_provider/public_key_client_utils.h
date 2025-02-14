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

#ifndef CPIO_CLIENT_PROVIDERS_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEY_CLIENT_UTILS_H_
#define CPIO_CLIENT_PROVIDERS_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEY_CLIENT_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "src/core/interface/http_types.h"
#include "src/core/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"
#include "src/public/cpio/proto/public_key_service/v1/public_key_service.pb.h"

#include "error_codes.h"

namespace google::scp::cpio::client_providers {
class PublicKeyClientUtils {
 public:
  /**
   * @brief Parse the expired time from http response headers.
   *
   * @param headers http response headers.
   * @param[out] expired_time_in_s the expired time in Unix time in seconds for
   * public keys.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ParseExpiredTimeFromHeaders(
      const core::HttpHeaders& headers, uint64_t& expired_time_in_s) noexcept;

  /**
   * @brief Parse Public keys from http response body.
   *
   * @param body http response body.
   * @param[out] public_keys Public keys.
   * @return core::ExecutionResult
   */
  static core::ExecutionResult ParsePublicKeysFromBody(
      const std::shared_ptr<std::string>& body,
      std::vector<cmrt::sdk::public_key_service::v1::PublicKey>&
          public_keys) noexcept;
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PUBLIC_KEY_CLIENT_PROVIDER_PUBLIC_KEY_CLIENT_UTILS_H_
