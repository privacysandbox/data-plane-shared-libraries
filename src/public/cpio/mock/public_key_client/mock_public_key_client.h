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

#ifndef PUBLIC_CPIO_MOCK_PUBLIC_KEY_CLIENT_MOCK_PUBLIC_KEY_CLIENT_H_
#define PUBLIC_CPIO_MOCK_PUBLIC_KEY_CLIENT_MOCK_PUBLIC_KEY_CLIENT_H_

#include <gmock/gmock.h>

#include "absl/status/status.h"
#include "src/public/cpio/interface/public_key_client/public_key_client_interface.h"

namespace google::scp::cpio {
class MockPublicKeyClient : public PublicKeyClientInterface {
 public:
  MockPublicKeyClient() {
    ON_CALL(*this, Init).WillByDefault(testing::Return(absl::OkStatus()));
    ON_CALL(*this, Run).WillByDefault(testing::Return(absl::OkStatus()));
    ON_CALL(*this, Stop).WillByDefault(testing::Return(absl::OkStatus()));
  }

  MOCK_METHOD(absl::Status, Init, (), (noexcept, override));
  MOCK_METHOD(absl::Status, Run, (), (noexcept, override));
  MOCK_METHOD(absl::Status, Stop, (), (noexcept, override));

  MOCK_METHOD(
      absl::Status, ListPublicKeys,
      (cmrt::sdk::public_key_service::v1::ListPublicKeysRequest request,
       Callback<cmrt::sdk::public_key_service::v1::ListPublicKeysResponse>
           callback),
      (noexcept, override));
};

}  // namespace google::scp::cpio

#endif  // PUBLIC_CPIO_MOCK_PUBLIC_KEY_CLIENT_MOCK_PUBLIC_KEY_CLIENT_H_
