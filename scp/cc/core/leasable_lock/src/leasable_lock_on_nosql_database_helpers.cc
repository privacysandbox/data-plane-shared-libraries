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

#include <mutex>
#include <shared_mutex>
#include <string_view>
#include <thread>

#include "absl/strings/numbers.h"
#include "core/common/time_provider/src/time_provider.h"
#include "scp/cc/core/leasable_lock/src/configuration_keys.h"
#include "scp/cc/core/leasable_lock/src/error_codes.h"
#include "scp/cc/core/leasable_lock/src/leasable_lock_on_nosql_database.h"

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
using google::scp::core::TimeDuration;
using google::scp::core::UpsertDatabaseItemRequest;
using google::scp::core::UpsertDatabaseItemResponse;
using google::scp::core::common::TimeProvider;
using std::atomic;
using std::get;
using std::make_shared;
using std::mutex;
using std::optional;
using std::shared_lock;
using std::shared_mutex;
using std::shared_ptr;
using std::unique_lock;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::this_thread::sleep_for;

namespace google::scp::core {
ExecutionResult LeasableLockOnNoSQLDatabase::ConstructAttributesFromLeaseInfo(
    const LeaseInfoInternal& lease,
    shared_ptr<std::vector<NoSqlDatabaseKeyValuePair>>& attributes) {
  NoSqlDatabaseKeyValuePair key_value1;
  key_value1.attribute_name =
      make_shared<std::string>(kPartitionLockTableLeaseOwnerIdAttributeName);
  key_value1.attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(
          lease.lease_owner_info.lease_acquirer_id);

  NoSqlDatabaseKeyValuePair key_value2;
  key_value2.attribute_name = make_shared<std::string>(
      kLockTableLeaseOwnerServiceEndpointAddressAttributeName);
  key_value2.attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(
          lease.lease_owner_info.service_endpoint_address);

  NoSqlDatabaseKeyValuePair key_value3;
  try {
    std::string lease_expiration_timestamp_string = std::to_string(
        lease.lease_expiration_timestamp_in_milliseconds.count());
    key_value3.attribute_name = make_shared<std::string>(
        kPartitionLockTableLeaseExpirationTimestampAttributeName);
    key_value3.attribute_value =
        make_shared<NoSQLDatabaseValidAttributeValueTypes>(
            lease_expiration_timestamp_string);
  } catch (...) {
    return FailureExecutionResult(
        core::errors::SC_LEASABLE_LOCK_TIMESTAMP_CONVERSION_ERROR);
  }

  attributes->push_back(key_value1);
  attributes->push_back(key_value2);
  attributes->push_back(key_value3);

  return SuccessExecutionResult();
}

ExecutionResult LeasableLockOnNoSQLDatabase::ObtainLeaseInfoFromAttributes(
    const shared_ptr<std::vector<NoSqlDatabaseKeyValuePair>>& attributes,
    LeaseInfoInternal& lease) {
  for (const auto& attribute : *attributes) {
    if (*attribute.attribute_name ==
        std::string_view(kPartitionLockTableLeaseOwnerIdAttributeName)) {
      lease.lease_owner_info.lease_acquirer_id =
          get<std::string>(*attribute.attribute_value);
    } else if (*attribute.attribute_name ==
               std::string_view(
                   kLockTableLeaseOwnerServiceEndpointAddressAttributeName)) {
      lease.lease_owner_info.service_endpoint_address =
          get<std::string>(*attribute.attribute_value);
    } else if (*attribute.attribute_name ==
               std::string_view(
                   kPartitionLockTableLeaseExpirationTimestampAttributeName)) {
      auto timestamp_string = get<std::string>(*attribute.attribute_value);
      int64_t timestamp_value = 0;
      if (!absl::SimpleAtoi(std::string_view(timestamp_string.c_str()),
                            &timestamp_value)) {
        return FailureExecutionResult(
            core::errors::SC_LEASABLE_LOCK_TIMESTAMP_CONVERSION_ERROR);
      }

      lease.lease_expiration_timestamp_in_milliseconds =
          milliseconds(timestamp_value);
    } else if (*attribute.attribute_name ==
               std::string_view(
                   kLockTableLeaseAcquisitionDisallowedAttributeName)) {
      auto value = get<std::string>(*attribute.attribute_value);
      if (value == "true" || value == "True") {
        lease.lease_acquisition_disallowed = true;
      }
    }
  }
  return SuccessExecutionResult();
}

ExecutionResult LeasableLockOnNoSQLDatabase::WriteLeaseSynchronouslyToDatabase(
    const LeaseInfoInternal& previous_lease,
    const LeaseInfoInternal& new_lease) {
  atomic<bool> request_executed = false;

  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      response_context;
  AsyncContext<UpsertDatabaseItemRequest, UpsertDatabaseItemResponse>
      request_context(make_shared<UpsertDatabaseItemRequest>(),
                      [&](auto& context) {
                        response_context = context;
                        request_executed = true;
                      });

  request_context.request->table_name = make_shared<std::string>(table_name_);
  request_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>();
  request_context.request->partition_key->attribute_name =
      make_shared<std::string>(kPartitionLockTableLockIdKeyName);
  request_context.request->partition_key->attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(lock_row_key_);

  // Old attributes (conditional statement)
  request_context.request->attributes =
      make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();
  auto result = ConstructAttributesFromLeaseInfo(
      previous_lease, request_context.request->attributes);
  if (!result.Successful()) {
    return result;
  }

  // New attributes
  request_context.request->new_attributes =
      make_shared<std::vector<NoSqlDatabaseKeyValuePair>>();
  result = ConstructAttributesFromLeaseInfo(
      new_lease, request_context.request->new_attributes);
  if (!result.Successful()) {
    return result;
  }

  result = database_->UpsertDatabaseItem(request_context);
  if (!result.Successful()) {
    return result;
  }

  // Wait for the query to be executed.
  while (!request_executed) {
    sleep_for(milliseconds(5));
  }

  if (!response_context.result.Successful()) {
    return response_context.result;
  }

  return SuccessExecutionResult();
}

ExecutionResult LeasableLockOnNoSQLDatabase::ReadLeaseSynchronouslyFromDatabase(
    LeaseInfoInternal& lease) {
  atomic<bool> request_executed = false;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse>
      response_context;
  AsyncContext<GetDatabaseItemRequest, GetDatabaseItemResponse> request_context(
      make_shared<GetDatabaseItemRequest>(), [&](auto& updated_context) {
        response_context = updated_context;
        request_executed = true;
      });

  request_context.request->table_name = make_shared<std::string>(table_name_);
  request_context.request->partition_key =
      make_shared<NoSqlDatabaseKeyValuePair>();
  request_context.request->partition_key->attribute_name =
      make_shared<std::string>(kPartitionLockTableLockIdKeyName);
  request_context.request->partition_key->attribute_value =
      make_shared<NoSQLDatabaseValidAttributeValueTypes>(lock_row_key_);

  auto result = database_->GetDatabaseItem(request_context);
  if (!result.Successful()) {
    return result;
  }

  // Wait for the query to be executed.
  while (!request_executed) {
    sleep_for(milliseconds(5));
  }

  if (!response_context.result.Successful()) {
    return response_context.result;
  }

  result = ObtainLeaseInfoFromAttributes(response_context.response->attributes,
                                         lease);
  if (!result.Successful()) {
    return result;
  }

  return SuccessExecutionResult();
}
}  // namespace google::scp::core
