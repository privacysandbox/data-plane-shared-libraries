// Copyright 2022 Google LLC
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

#include <gtest/gtest.h>

#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider_no_overrides.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "scp/cc/core/leasable_lock/src/error_codes.h"
#include "scp/cc/core/leasable_lock/src/leasable_lock_on_nosql_database.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::common::TimeProvider;
using google::scp::core::test::ResultIs;

static constexpr char kPartitionLockTableDefaultName[] =
    "core_ll_partition_lock_table";

namespace google::scp::core::test {
template <class... Args>
class LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester
    : public LeasableLockOnNoSQLDatabase {
 public:
  explicit LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester(Args... args)
      : LeasableLockOnNoSQLDatabase(args...) {}

  void LeaseInfoInternalTestIsExpired() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal(lease_info);
    EXPECT_TRUE(lease_info_internal.IsExpired());

    lease_info_internal = LeaseInfoInternal(
        lease_info, std::chrono::duration_cast<std::chrono::milliseconds>(
                        TimeProvider::GetWallTimestampInNanoseconds()) +
                        std::chrono::seconds(1));
    EXPECT_FALSE(lease_info_internal.IsExpired());
  }

  void LeaseInfoInternalSetExpirationTimestampFromNow() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal(lease_info);
    EXPECT_TRUE(lease_info_internal.IsExpired());

    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::milliseconds(500));
    EXPECT_FALSE(lease_info_internal.IsExpired());

    std::this_thread::sleep_for(std::chrono::seconds(1));
    EXPECT_TRUE(lease_info_internal.IsExpired());
  }

  void LeaseInfoInternalTestIsLeaseOwner() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal1(lease_info);
    EXPECT_TRUE(lease_info_internal1.IsLeaseOwner("1"));

    LeaseInfoInternal lease_info_internal2(lease_info);
    EXPECT_FALSE(lease_info_internal2.IsLeaseOwner("2"));
  }

  void LeaseInfoInternalTestIsLeaseRenewalRequired() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";

    LeaseInfoInternal lease_info_internal(lease_info);
    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::milliseconds(500));

    EXPECT_FALSE(lease_info_internal.IsLeaseRenewalRequired(
        std::chrono::milliseconds(500), 50));
    EXPECT_FALSE(lease_info_internal.IsLeaseRenewalRequired(
        std::chrono::milliseconds(900), 50));
    EXPECT_TRUE(lease_info_internal.IsLeaseRenewalRequired(
        std::chrono::milliseconds(1100), 50));

    lease_info_internal.SetExpirationTimestampFromNow(std::chrono::seconds(50));
    EXPECT_FALSE(lease_info_internal.IsLeaseRenewalRequired(
        std::chrono::milliseconds(10), 50));

    lease_info_internal.SetExpirationTimestampFromNow(std::chrono::seconds(0));
    EXPECT_TRUE(lease_info_internal.IsLeaseRenewalRequired(
        std::chrono::seconds(10), 50));
  }

  void LeaseInfoInternalTestIsHalfLeaseDurationPassed() {
    LeaseInfo lease_info;
    lease_info.lease_acquirer_id = "1";
    lease_info.service_endpoint_address = "10.1.1.1";
    auto lease_duration_in_ms = std::chrono::milliseconds(10000);  // 10 sec

    LeaseInfoInternal lease_info_internal(lease_info);
    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::seconds(10));  // 10 s left in the current lease.
    EXPECT_FALSE(
        lease_info_internal.IsHalfLeaseDurationPassed(lease_duration_in_ms));

    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::seconds(2));  // 2 s left in the current lease.
    EXPECT_TRUE(
        lease_info_internal.IsHalfLeaseDurationPassed(lease_duration_in_ms));

    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::seconds(6));  // 6 s left in the current lease.
    EXPECT_FALSE(
        lease_info_internal.IsHalfLeaseDurationPassed(lease_duration_in_ms));

    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::seconds(4));  // 4 s left in the current lease.
    EXPECT_TRUE(
        lease_info_internal.IsHalfLeaseDurationPassed(lease_duration_in_ms));

    lease_info_internal.SetExpirationTimestampFromNow(
        std::chrono::milliseconds(0));  // Lease expired already.
    EXPECT_TRUE(
        lease_info_internal.IsHalfLeaseDurationPassed(lease_duration_in_ms));
  }
};

TEST(LeasableLockLeaseInfoTest, LeaseInfoInternalTestIsExpired) {
  LeaseInfo lease_acquirer_1_;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_1_, kPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsExpired();
}

TEST(LeasableLockLeaseInfoTest,
     LeaseInfoInternalSetExpirationTimestampFromNow) {
  LeaseInfo lease_acquirer_1_;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_1_, kPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalSetExpirationTimestampFromNow();
}

TEST(LeasableLockLeaseInfoTest, LeaseInfoInternalTestIsLeaseOwner) {
  LeaseInfo lease_acquirer_1_;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_1_, kPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsLeaseOwner();
}

TEST(LeasableLockLeaseInfoTest, LeaseInfoInternalTestIsLeaseRenewalRequired) {
  LeaseInfo lease_acquirer_1_;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_1_, kPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsLeaseRenewalRequired();
}

TEST(LeasableLockLeaseInfoTest,
     LeaseInfoInternalTestIsHalfLeaseDurationPassed) {
  LeaseInfo lease_acquirer_1_;
  LeasableLockOnNoSQLDatabaseLeaseInfoInternalTester leasable_lock(
      nullptr, lease_acquirer_1_, kPartitionLockTableDefaultName);
  leasable_lock.LeaseInfoInternalTestIsHalfLeaseDurationPassed();
}
}  // namespace google::scp::core::test
