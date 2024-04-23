//  Copyright 2023 Google LLC
//
//  Licensed under the Apache License, Version 2.0 (the "License");
//  you may not use this file except in compliance with the License.
//  You may obtain a copy of the License at
//
//       http://www.apache.org/licenses/LICENSE-2.0
//
//  Unless required by applicable law or agreed to in writing, software
//  distributed under the License is distributed on an "AS IS" BASIS,
//  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//  See the License for the specific language governing permissions and
//  limitations under the License.

#include "src/metric/key_fetch.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

namespace privacy_sandbox::server_common {
namespace {

using testing::Pair;
using testing::UnorderedElementsAre;

TEST(KeyFetchResultInstrumentation, GetKeyFetchFailureCount) {
  EXPECT_THAT(KeyFetchResultCounter::GetKeyFetchFailureCount(),
              UnorderedElementsAre(
                  Pair("public key sync", 0), Pair("public key async", 0),
                  Pair("private key sync", 0), Pair("private key async", 0)));

  KeyFetchResultCounter::IncrementPublicKeyFetchSyncFailureCount();
  KeyFetchResultCounter::IncrementPublicKeyFetchAsyncFailureCount();
  EXPECT_THAT(KeyFetchResultCounter::GetKeyFetchFailureCount(),
              UnorderedElementsAre(
                  Pair("public key sync", 1), Pair("public key async", 1),
                  Pair("private key sync", 0), Pair("private key async", 0)));

  KeyFetchResultCounter::IncrementPrivateKeyFetchSyncFailureCount();
  KeyFetchResultCounter::IncrementPrivateKeyFetchAsyncFailureCount();
  EXPECT_THAT(KeyFetchResultCounter::GetKeyFetchFailureCount(),
              UnorderedElementsAre(
                  Pair("public key sync", 1), Pair("public key async", 1),
                  Pair("private key sync", 1), Pair("private key async", 1)));
}

TEST(KeyFetchResultInstrumentation, GetNumKeysParsed) {
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysParsed(),
      UnorderedElementsAre(Pair("GCP public key", 0), Pair("AWS public key", 0),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPublicKeysParsed(CloudPlatform::kGcp, 5);
  KeyFetchResultCounter::SetNumPublicKeysParsed(CloudPlatform::kAws, 4);
  // Should do nothing.
  KeyFetchResultCounter::SetNumPublicKeysParsed(CloudPlatform::kLocal, 5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysParsed(),
      UnorderedElementsAre(Pair("GCP public key", 5), Pair("AWS public key", 4),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPrivateKeysParsed(5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysParsed(),
      UnorderedElementsAre(Pair("GCP public key", 5), Pair("AWS public key", 4),
                           Pair("private key", 5)));
}

TEST(KeyFetchResultInstrumentation, GetNumKeysCached) {
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysCached(),
      UnorderedElementsAre(Pair("GCP public key", 0), Pair("AWS public key", 0),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPublicKeysCached(CloudPlatform::kGcp, 5);
  KeyFetchResultCounter::SetNumPublicKeysCached(CloudPlatform::kAws, 4);
  // Should do nothing.
  KeyFetchResultCounter::SetNumPublicKeysCached(CloudPlatform::kLocal, 5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysCached(),
      UnorderedElementsAre(Pair("GCP public key", 5), Pair("AWS public key", 4),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPrivateKeysCached(5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysCached(),
      UnorderedElementsAre(Pair("GCP public key", 5), Pair("AWS public key", 4),
                           Pair("private key", 5)));
}

TEST(KeyFetchResultInstrumentation, GetNumPrivateKeysFetched) {
  EXPECT_THAT(KeyFetchResultCounter::GetNumPrivateKeysFetched(),
              UnorderedElementsAre(Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPrivateKeysFetched(5);
  EXPECT_THAT(KeyFetchResultCounter::GetNumPrivateKeysFetched(),
              UnorderedElementsAre(Pair("private key", 5)));

  KeyFetchResultCounter::SetNumPrivateKeysFetched(1);
  EXPECT_THAT(KeyFetchResultCounter::GetNumPrivateKeysFetched(),
              UnorderedElementsAre(Pair("private key", 1)));
}

}  // namespace
}  // namespace privacy_sandbox::server_common
