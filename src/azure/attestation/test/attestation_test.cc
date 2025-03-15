// Portions Copyright (c) Microsoft Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/azure/attestation/src/attestation.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;

namespace google::scp::cc::azure::attestation::test {

class JsonAttestationReportTest : public ::testing::Test {
 protected:
  void SetUp() override {}

  void TearDown() override {}
};

TEST_F(JsonAttestationReportTest, FetchFakeAttestation) {
  EXPECT_TRUE(fetchFakeSnpAttestation().has_value());
}

TEST_F(JsonAttestationReportTest, FetchRealAttestation) {
  if (!hasSnp()) {
    return;
  }
  EXPECT_TRUE(fetchSnpAttestation().has_value());
}

TEST_F(JsonAttestationReportTest, FetchRealAttestationNormalReportData) {
  if (!hasSnp()) {
    return;
  }
  EXPECT_TRUE(fetchSnpAttestation("example_report_data").has_value());
}

TEST_F(JsonAttestationReportTest, FetchRealAttestationLongReportData) {
  if (!hasSnp()) {
    return;
  }
  EXPECT_TRUE(fetchSnpAttestation("a_very_long_report_data_string_which_is_so_"
                                  "long_that_it_exceeds_report_"
                                  "data_length")
                  .has_value());
}

TEST_F(JsonAttestationReportTest, FetchRealAttestationNonSnp) {
  if (hasSnp()) {
    return;
  }
  EXPECT_FALSE(fetchSnpAttestation().has_value());
}
}  // namespace google::scp::cc::azure::attestation::test
