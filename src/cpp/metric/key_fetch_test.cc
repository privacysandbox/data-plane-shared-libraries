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

#include "src/cpp/metric/key_fetch.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace privacy_sandbox::server_common {
namespace {

using testing::Pair;
using testing::UnorderedElementsAre;

TEST(KeyFetchResultInstrumentation, GetKeyFetchFailureCount) {
  EXPECT_THAT(KeyFetchResultCounter::GetKeyFetchFailureCount(),
              UnorderedElementsAre(Pair("public key dispatch", 0),
                                   Pair("public key async", 0),
                                   Pair("private key dispatch", 0),
                                   Pair("private key async", 0)));

  KeyFetchResultCounter::IncrementPublicKeyFetchDispatchFailureCount();
  KeyFetchResultCounter::IncrementPublicKeyFetchAsyncFailureCount();
  EXPECT_THAT(KeyFetchResultCounter::GetKeyFetchFailureCount(),
              UnorderedElementsAre(Pair("public key dispatch", 1),
                                   Pair("public key async", 1),
                                   Pair("private key dispatch", 0),
                                   Pair("private key async", 0)));

  KeyFetchResultCounter::IncrementPrivateKeyFetchDispatchFailureCount();
  KeyFetchResultCounter::IncrementPrivateKeyFetchAsyncFailureCount();
  EXPECT_THAT(KeyFetchResultCounter::GetKeyFetchFailureCount(),
              UnorderedElementsAre(Pair("public key dispatch", 1),
                                   Pair("public key async", 1),
                                   Pair("private key dispatch", 1),
                                   Pair("private key async", 1)));
}

TEST(KeyFetchResultInstrumentation, GetNumKeysParsedOnRecentFetch) {
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysParsedOnRecentFetch(),
      UnorderedElementsAre(Pair("public key GCP", 0), Pair("public key AWS", 0),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPublicKeysParsedOnRecentFetch(
      CloudPlatform::kGcp, 5);
  KeyFetchResultCounter::SetNumPublicKeysParsedOnRecentFetch(
      CloudPlatform::kAws, 4);
  // Should do nothing.
  KeyFetchResultCounter::SetNumPublicKeysParsedOnRecentFetch(
      CloudPlatform::kLocal, 5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysParsedOnRecentFetch(),
      UnorderedElementsAre(Pair("public key GCP", 5), Pair("public key AWS", 4),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPrivateKeysParsedOnRecentFetch(5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysParsedOnRecentFetch(),
      UnorderedElementsAre(Pair("public key GCP", 5), Pair("public key AWS", 4),
                           Pair("private key", 5)));
}

TEST(KeyFetchResultInstrumentation, GetNumKeysCachedAfterRecentFetch) {
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysCachedAfterRecentFetch(),
      UnorderedElementsAre(Pair("public key GCP", 0), Pair("public key AWS", 0),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPublicKeysCachedAfterRecentFetch(
      CloudPlatform::kGcp, 5);
  KeyFetchResultCounter::SetNumPublicKeysCachedAfterRecentFetch(
      CloudPlatform::kAws, 4);
  // Should do nothing.
  KeyFetchResultCounter::SetNumPublicKeysCachedAfterRecentFetch(
      CloudPlatform::kLocal, 5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysCachedAfterRecentFetch(),
      UnorderedElementsAre(Pair("public key GCP", 5), Pair("public key AWS", 4),
                           Pair("private key", 0)));

  KeyFetchResultCounter::SetNumPrivateKeysCachedAfterRecentFetch(5);
  EXPECT_THAT(
      KeyFetchResultCounter::GetNumKeysCachedAfterRecentFetch(),
      UnorderedElementsAre(Pair("public key GCP", 5), Pair("public key AWS", 4),
                           Pair("private key", 5)));
}

}  // namespace
}  // namespace privacy_sandbox::server_common
