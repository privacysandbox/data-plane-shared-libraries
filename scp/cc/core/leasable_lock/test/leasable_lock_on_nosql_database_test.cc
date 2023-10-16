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

#include "scp/cc/core/leasable_lock/src/leasable_lock_on_nosql_database.h"

#include <gtest/gtest.h>

#include <sys/types.h>

#include <cstdint>

#include "absl/strings/numbers.h"
#include "core/common/time_provider/src/time_provider.h"
#include "core/interface/nosql_database_provider_interface.h"
#include "core/nosql_database_provider/mock/mock_nosql_database_provider_no_overrides.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "scp/cc/core/leasable_lock/src/error_codes.h"

using google::scp::core::AsyncContext;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::GetDatabaseItemRequest;
using google::scp::core::GetDatabaseItemResponse;
using google::scp::core::LeaseInfo;
using google::scp::core::LeaseManagerInterface;
using google::scp::core::LeaseTransitionType;
using google::scp::core::NoSQLDatabaseAttributeName;
using google::scp::core::NoSqlDatabaseKeyValuePair;
using google::scp::core::NoSQLDatabaseProviderInterface;
using google::scp::core::NoSQLDatabaseValidAttributeValueTypes;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::common::TimeProvider;
using google::scp::core::nosql_database_provider::mock::
    MockNoSQLDatabaseProviderNoOverrides;
using google::scp::core::test::ResultIs;

static constexpr char kPartitionLockTableDefaultName[] =
    "core_ll_partition_lock_table";

namespace google::scp::core::test {

static const std::vector<NoSqlDatabaseKeyValuePair> kDummyLockRowAttributes = {
    {std::make_shared<NoSQLDatabaseAttributeName>(
         kPartitionLockTableLeaseOwnerIdAttributeName),
     std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr1")},
    {std::make_shared<NoSQLDatabaseAttributeName>(
         kLockTableLeaseOwnerServiceEndpointAddressAttributeName),
     std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr2")},
    {std::make_shared<NoSQLDatabaseAttributeName>(
         kPartitionLockTableLeaseExpirationTimestampAttributeName),
     std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("0")}};

static const std::vector<NoSqlDatabaseKeyValuePair>
    kDummyLockRowAttributesWithLeaseAcquisitionDisallowed = {
        {std::make_shared<NoSQLDatabaseAttributeName>(
             kPartitionLockTableLeaseOwnerIdAttributeName),
         std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr1")},
        {std::make_shared<NoSQLDatabaseAttributeName>(
             kLockTableLeaseOwnerServiceEndpointAddressAttributeName),
         std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("attr2")},
        {std::make_shared<NoSQLDatabaseAttributeName>(
             kPartitionLockTableLeaseExpirationTimestampAttributeName),
         std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("0")},
        {std::make_shared<NoSQLDatabaseAttributeName>(
             kLockTableLeaseAcquisitionDisallowedAttributeName),
         std::make_shared<NoSQLDatabaseValidAttributeValueTypes>("true")}};

void SetOverridesOnMockNoSQLDatabase(
    const std::shared_ptr<MockNoSQLDatabaseProviderNoOverrides>&
        mock_nosql_database_provider_,
    const LeaseInfo& lease_info,
    std::chrono::milliseconds lease_expiration_timestamp) {
  ON_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillByDefault([=](AsyncContext<GetDatabaseItemRequest,
                                      GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();
        context.response->attributes->push_back(
            {std::make_shared<NoSQLDatabaseAttributeName>(
                 kPartitionLockTableLeaseOwnerIdAttributeName),
             std::make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                 lease_info.lease_acquirer_id)});
        context.response->attributes->push_back(
            {std::make_shared<NoSQLDatabaseAttributeName>(
                 kLockTableLeaseOwnerServiceEndpointAddressAttributeName),
             std::make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                 lease_info.service_endpoint_address)});
        context.response->attributes->push_back(
            {std::make_shared<NoSQLDatabaseAttributeName>(
                 kPartitionLockTableLeaseExpirationTimestampAttributeName),
             std::make_shared<NoSQLDatabaseValidAttributeValueTypes>(
                 std::to_string(lease_expiration_timestamp.count()))});

        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  ON_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillByDefault([=](AsyncContext<UpsertDatabaseItemRequest,
                                      UpsertDatabaseItemResponse>& context) {
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
}

template <class... Args>
class LeasableLockOnNoSQLDatabasePrivate : public LeasableLockOnNoSQLDatabase {
 public:
  explicit LeasableLockOnNoSQLDatabasePrivate(Args... args)
      : LeasableLockOnNoSQLDatabase(args...) {}

  void SetCachedCurrentLeaseOwner(
      LeaseInfo& lease_owner_info,
      std::chrono::milliseconds lease_expiration_timestamp) {
    LeaseInfoInternal lease_info;
    lease_info.lease_owner_info = lease_owner_info;
    lease_info.lease_expiration_timestamp_in_milliseconds =
        lease_expiration_timestamp;
    current_lease_ = lease_info;
  }

  // Should be used only when the current_lease_ is valid
  std::chrono::milliseconds GetCurrentLeaseExpirationTimestamp() {
    if (current_lease_.has_value()) {
      return current_lease_->lease_expiration_timestamp_in_milliseconds;
    }
    return std::chrono::milliseconds(0);
  }

  bool IsLeaseCached() { return current_lease_.has_value(); }
};

class LeasableLockOnNoSQLDatabaseTest : public ::testing::Test {
 protected:
  LeasableLockOnNoSQLDatabaseTest() {
    // Two different acquirers
    lease_acquirer_1_ = LeaseInfo{"123", "10.1.1.1"};
    lease_acquirer_2_ = LeaseInfo{"456", "10.1.1.2"};
    mock_nosql_database_provider_ =
        std::make_shared<MockNoSQLDatabaseProviderNoOverrides>();
    nosql_database_provider_ = mock_nosql_database_provider_;
  }

  LeaseInfo lease_acquirer_1_;
  LeaseInfo lease_acquirer_2_;
  size_t lease_renewal_percent_time_left_ = 80;
  std::chrono::milliseconds lease_duration_in_ms_ =
      std::chrono::milliseconds(1500);
  std::string leasable_lock_key_ = "0";
  std::string lease_table_name_ = kPartitionLockTableDefaultName;
  std::shared_ptr<MockNoSQLDatabaseProviderNoOverrides>
      mock_nosql_database_provider_;
  std::shared_ptr<NoSQLDatabaseProviderInterface> nosql_database_provider_;
};

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       InitializeAndObtainConfiguredLeaseDurationIsSuccessful) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_EQ(leasable_lock.GetConfiguredLeaseDurationInMilliseconds(),
            lease_duration_in_ms_.count());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsFalseAfterInitialization) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueAfterInitialization) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseReadsAndUpsertsLockRowWithReadValueAsPreconditionValue) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillOnce([&](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
        EXPECT_EQ(context.request->attributes->size(), 3);
        EXPECT_EQ(*kDummyLockRowAttributes[0].attribute_name,
                  *context.request->attributes->at(0).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[1].attribute_name,
                  *context.request->attributes->at(1).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[2].attribute_name,
                  *context.request->attributes->at(2).attribute_name);
        EXPECT_EQ(
            std::get<std::string>(*kDummyLockRowAttributes[0].attribute_value),
            std::get<std::string>(
                *context.request->attributes->at(0).attribute_value));
        EXPECT_EQ(
            std::get<std::string>(*kDummyLockRowAttributes[1].attribute_value),
            std::get<std::string>(
                *context.request->attributes->at(1).attribute_value));
        EXPECT_EQ(
            std::get<std::string>(*kDummyLockRowAttributes[2].attribute_value),
            std::get<std::string>(
                *context.request->attributes->at(2).attribute_value));

        int64_t timestamp = -1;
        EXPECT_TRUE(absl::SimpleAtoi(
            std::get<std::string>(
                *context.request->attributes->at(2).attribute_value),
            &timestamp));
        EXPECT_EQ(timestamp, 0);

        EXPECT_EQ(context.request->new_attributes->size(), 3);
        EXPECT_EQ(*kDummyLockRowAttributes[0].attribute_name,
                  *context.request->new_attributes->at(0).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[1].attribute_name,
                  *context.request->new_attributes->at(1).attribute_name);
        EXPECT_EQ(*kDummyLockRowAttributes[2].attribute_name,
                  *context.request->new_attributes->at(2).attribute_name);
        EXPECT_EQ(std::get<std::string>(
                      *context.request->new_attributes->at(0).attribute_value),
                  lease_acquirer_1_.lease_acquirer_id);
        EXPECT_EQ(std::get<std::string>(
                      *context.request->new_attributes->at(1).attribute_value),
                  lease_acquirer_1_.service_endpoint_address);
        EXPECT_TRUE(absl::SimpleAtoi(
            std::get<std::string>(
                *context.request->new_attributes->at(2).attribute_value),
            &timestamp));
        EXPECT_GT(timestamp, 0);
        EXPECT_EQ(context.request->new_attributes->size(), 3);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh*/));
  EXPECT_TRUE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseFailsIfLeaseAcquisitionIsDisallowed) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributesWithLeaseAcquisitionDisallowed);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_EQ(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            FailureExecutionResult(
                core::errors::SC_LEASABLE_LOCK_ACQUISITION_DISALLOWED));
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfReadLockRowRequestFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();
        context.result = SuccessExecutionResult();
        return FailureExecutionResult(SC_UNKNOWN);
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfReadLockRowFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.callback(context);
        return SuccessExecutionResult();
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfWriteLockRowRequestFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });

  EXPECT_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillOnce([&](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
        context.result = SuccessExecutionResult();
        return FailureExecutionResult(SC_UNKNOWN);
      });

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotCacheIfWriteLockRowFails) {
  EXPECT_CALL(*mock_nosql_database_provider_, GetDatabaseItem)
      .WillOnce([&](AsyncContext<GetDatabaseItemRequest,
                                 GetDatabaseItemResponse>& context) {
        context.response = std::make_shared<GetDatabaseItemResponse>();
        context.response->attributes =
            std::make_shared<std::vector<NoSqlDatabaseKeyValuePair>>(
                kDummyLockRowAttributes);
        context.result = SuccessExecutionResult();
        context.callback(context);
        return SuccessExecutionResult();
      });
  EXPECT_CALL(*mock_nosql_database_provider_, UpsertDatabaseItem)
      .WillOnce([&](AsyncContext<UpsertDatabaseItemRequest,
                                 UpsertDatabaseItemResponse>& context) {
        context.result = FailureExecutionResult(SC_UNKNOWN);
        context.callback(context);
        return SuccessExecutionResult();
      });
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
  EXPECT_NE(leasable_lock.RefreshLease(false /*is_read_only_lease_refresh*/),
            SuccessExecutionResult());
  EXPECT_FALSE(leasable_lock.IsLeaseCached());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfOwningLeaseIsExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  // Expired lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() -
                             std::chrono::milliseconds(1)));
  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsFalseIfNonOwningLeaseIsNotExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  // Not expired lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(100)));

  EXPECT_FALSE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfNonOwningLeaseIsExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  // Expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() -
                             std::chrono::seconds(1)));

  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsFalseIfOwningLeaseHasNotMetRenewThreshold) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(6)));

  EXPECT_FALSE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfOwningLeaseHasMetRenewThreshold) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(1)));

  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsTrueIfNonOwningLeaseHasStaleCachedLease) {
  auto lease_duration = std::chrono::seconds(15);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration, lease_renewal_percent_time_left_);

  // Non expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(2)));

  EXPECT_TRUE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       ShouldRefreshLeaseIsFalseIfNonOwningLeaseHasFreshCachedLease) {
  auto lease_duration = std::chrono::seconds(15);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration, lease_renewal_percent_time_left_);

  // Non expired lease and non owner lease
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(9)));

  EXPECT_FALSE(leasable_lock.ShouldRefreshLease());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsTrueIfLeaseOwnerIsCurrent) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(100)));

  EXPECT_TRUE(leasable_lock.IsCurrentLeaseOwner());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsFalseIfLeaseOwnerIsOther) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() +
                             std::chrono::seconds(100)));

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       IsCurrentLeaseOwnerReturnsFalseIfLeaseOwnerIsCurrentAndExpired) {
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, std::chrono::duration_cast<std::chrono::milliseconds>(
                             TimeProvider::GetWallTimestampInNanoseconds() -
                             std::chrono::seconds(1)));

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseForFirstTimeOwnerAndExpired) {
  auto current_lease_owner_lease_expiration =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() -
          std::chrono::seconds(100));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_1_,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // New lease owner is the other lease owner.
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
}

TEST_F(
    LeasableLockOnNoSQLDatabaseTest,
    RefreshLeaseRefreshesTheCachedLeaseForFirstTimeNotOwnerAndLeaseNotExpired) {
  auto current_lease_owner_lease_expiration =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() +
          std::chrono::seconds(100));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  current_lease_owner_lease_expiration);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // New lease owner is the other lease owner.
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseForFirstTimeNotOwnerAndButExpired) {
  auto current_lease_owner_lease_expiration =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() -
          std::chrono::seconds(1));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  current_lease_owner_lease_expiration);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseIfOwner) {
  auto current_lease_owner_lease_expiration =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() -
          std::chrono::seconds(1));
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_1_,
                                  current_lease_owner_lease_expiration);
  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);

  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_1_, current_lease_owner_lease_expiration);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo().has_value(), false);
  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
  EXPECT_GT(leasable_lock.GetCurrentLeaseExpirationTimestamp(),
            current_lease_owner_lease_expiration);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseRefreshesTheCachedLeaseIfNonOwnerAndExpired) {
  std::chrono::milliseconds initial_expired_lease_expiration_timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() -
          std::chrono::milliseconds(1));
  // Initially the database has a lease of other lease acquirer for +10
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  initial_expired_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  leasable_lock.SetCachedCurrentLeaseOwner(
      lease_acquirer_2_, initial_expired_lease_expiration_timestamp);

  // Current lease acquirer does not own the lease.
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // Current lease acquirer owns the lease.
  EXPECT_TRUE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_1_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_1_.service_endpoint_address);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest,
       RefreshLeaseDoesNotRefreshTheCachedLeaseIfNonOwnerAndNotExpired) {
  auto initial_lease_expiration_timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() +
          std::chrono::seconds(10));

  // Initially the database has a lease of other lease acquirer for +10
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  initial_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_1_, lease_table_name_,
      leasable_lock_key_, lease_duration_in_ms_,
      lease_renewal_percent_time_left_);
  leasable_lock.SetCachedCurrentLeaseOwner(lease_acquirer_2_,
                                           initial_lease_expiration_timestamp);

  // Current lease acquirer does not own the lease.
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
  auto prev_lease_expiration_timestamp =
      leasable_lock.GetCurrentLeaseExpirationTimestamp();
  EXPECT_EQ(prev_lease_expiration_timestamp,
            initial_lease_expiration_timestamp);

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(false /* is_read_only_lease_refresh */));

  // Current lease acquirer still does not own the lease.
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_TRUE(leasable_lock.GetCurrentLeaseOwnerInfo().has_value());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
  auto current_lease_expiration_timestamp =
      leasable_lock.GetCurrentLeaseExpirationTimestamp();

  // Lease expiration timestamp is not changed.
  EXPECT_EQ(current_lease_expiration_timestamp,
            prev_lease_expiration_timestamp);
}

TEST_F(LeasableLockOnNoSQLDatabaseTest, LeaseIsNotWrittenIfReadOnlyIsTrue) {
  LeaseInfo lease_acquirer_2_ = {"2", "2.1.1.1"};
  LeaseInfo lease_acquirer_info_next = {"1", "10.1.1.1"};
  std::chrono::milliseconds initial_lease_expiration_timestamp =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          TimeProvider::GetWallTimestampInNanoseconds() +
          std::chrono::seconds(100));
  // Initially the database has a lease of other lease acquirer for +10
  SetOverridesOnMockNoSQLDatabase(mock_nosql_database_provider_,
                                  lease_acquirer_2_,
                                  initial_lease_expiration_timestamp);

  LeasableLockOnNoSQLDatabasePrivate leasable_lock(
      mock_nosql_database_provider_, lease_acquirer_info_next,
      kPartitionLockTableDefaultName, "0", std::chrono::seconds(15), 80);
  leasable_lock.SetCachedCurrentLeaseOwner(lease_acquirer_2_,
                                           initial_lease_expiration_timestamp);

  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());

  EXPECT_SUCCESS(
      leasable_lock.RefreshLease(true /* is_read_only_lease_refresh */));
  // Lease cannot be owned due to readonly refresh. Lease holder will be
  // 'lease_acquirer_2_'
  EXPECT_FALSE(leasable_lock.IsCurrentLeaseOwner());
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->lease_acquirer_id,
            lease_acquirer_2_.lease_acquirer_id);
  EXPECT_EQ(leasable_lock.GetCurrentLeaseOwnerInfo()->service_endpoint_address,
            lease_acquirer_2_.service_endpoint_address);
}

}  // namespace google::scp::core::test
