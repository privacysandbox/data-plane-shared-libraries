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

#include "scp/cc/cpio/client_providers/job_client_provider/src/job_client_utils.h"

#include <gtest/gtest.h>

#include <memory>
#include <string>

#include "core/common/serialization/src/serialization.h"
#include "core/interface/async_context.h"
#include "core/test/utils/proto_test_utils.h"
#include "core/utils/src/base64.h"
#include "cpio/client_providers/job_client_provider/test/hello_world.pb.h"
#include "google/protobuf/util/json_util.h"
#include "google/protobuf/util/time_util.h"
#include "public/core/interface/execution_result.h"
#include "public/core/test/interface/execution_result_matchers.h"
#include "public/cpio/proto/job_service/v1/job_service.pb.h"
#include "scp/cc/cpio/client_providers/job_client_provider/src/error_codes.h"

using google::cmrt::sdk::job_service::v1::Job;
using google::cmrt::sdk::job_service::v1::JobStatus;
using google::cmrt::sdk::nosql_database_service::v1::CreateDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::GetDatabaseItemRequest;
using google::cmrt::sdk::nosql_database_service::v1::Item;
using google::cmrt::sdk::nosql_database_service::v1::UpsertDatabaseItemRequest;
using google::protobuf::Any;
using google::protobuf::Duration;
using google::protobuf::util::MessageToJsonString;
using google::protobuf::util::TimeUtil;
using google::scp::core::ExecutionResult;
using google::scp::core::FailureExecutionResult;
using google::scp::core::SuccessExecutionResult;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS;
using google::scp::core::errors::SC_JOB_CLIENT_PROVIDER_SERIALIZATION_FAILED;
using google::scp::core::test::EqualsProto;
using google::scp::core::test::ResultIs;
using google::scp::core::utils::Base64Decode;
using google::scp::core::utils::Base64Encode;
using helloworld::HelloWorld;
using std::tuple;

namespace {
constexpr char kHelloWorldName[] = "hello";
constexpr int kHelloWorldId = 55678413;
const google::protobuf::Timestamp kHelloWorldProtoCreatedTime =
    TimeUtil::SecondsToTimestamp(1672531200);

constexpr char kJobId[] = "job-id";
constexpr char kServerJobId[] = "server-job-id";
constexpr char kJobsTableName[] = "Jobs";
constexpr char kJobsTablePartitionKeyName[] = "JobId";
constexpr char kServerJobIdColumnName[] = "ServerJobId";
constexpr char kJobBodyColumnName[] = "JobBody";
constexpr char kJobStatusColumnName[] = "JobStatus";
constexpr char kCreatedTimeColumnName[] = "CreatedTime";
constexpr char kUpdatedTimeColumnName[] = "UpdatedTime";
constexpr char kRetryCountColumnName[] = "RetryCount";
constexpr char kProcessingStartedTimeColumnName[] = "ProcessingStartedTime";

Any CreateHelloWorldProtoAsAny(google::protobuf::Timestamp created_time) {
  HelloWorld hello_world_input;
  hello_world_input.set_name(kHelloWorldName);
  hello_world_input.set_id(kHelloWorldId);
  *hello_world_input.mutable_created_time() = created_time;

  Any any;
  any.PackFrom(hello_world_input);
  return any;
}

std::string CreateHelloWorldProtoAsJsonString() {
  HelloWorld hello_world_input;
  hello_world_input.set_name(kHelloWorldName);
  hello_world_input.set_id(kHelloWorldId);
  *hello_world_input.mutable_created_time() = kHelloWorldProtoCreatedTime;

  std::string json_string;
  MessageToJsonString(hello_world_input, &json_string);
  return json_string;
}

Item CreateJobAsDatabaseItem(
    const std::string& job_body, const JobStatus& job_status,
    const google::protobuf::Timestamp& current_time,
    const google::protobuf::Timestamp& updated_time, const int retry_count,
    const google::protobuf::Timestamp& processing_started_time) {
  Item item;
  *item.mutable_key()->mutable_partition_key() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kJobsTablePartitionKeyName, kJobId);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kServerJobIdColumnName, kServerJobId);

  std::string encoded_job_body;
  Base64Encode(job_body, encoded_job_body);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kJobBodyColumnName, encoded_job_body);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeIntAttribute(
          kJobStatusColumnName, job_status);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kCreatedTimeColumnName, TimeUtil::ToString(current_time));
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kUpdatedTimeColumnName, TimeUtil::ToString(updated_time));
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeIntAttribute(
          kRetryCountColumnName, retry_count);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kProcessingStartedTimeColumnName,
          TimeUtil::ToString(processing_started_time));
  return item;
}
}  // namespace

namespace google::scp::cpio::client_providers::test {

class JobClientUtilsTest : public ::testing::TestWithParam<
                               tuple<JobStatus, JobStatus, ExecutionResult>> {
 public:
  JobStatus GetCurrentStatus() const { return std::get<0>(GetParam()); }

  JobStatus GetUpdateStatus() const { return std::get<1>(GetParam()); }

  ExecutionResult GetExpectedExecutionResult() const {
    return std::get<2>(GetParam());
  }
};

TEST(JobClientUtilsTest, MakeStringAttribute) {
  auto name = "name";
  auto value = "value";
  auto item_attribute = JobClientUtils::MakeStringAttribute(name, value);

  EXPECT_EQ(item_attribute.name(), name);
  EXPECT_EQ(item_attribute.value_string(), value);
}

TEST(JobClientUtilsTest, MakeIntAttribute) {
  auto name = "name";
  auto value = 5;
  auto item_attribute = JobClientUtils::MakeIntAttribute(name, value);

  EXPECT_EQ(item_attribute.name(), name);
  EXPECT_EQ(item_attribute.value_int(), value);
}

TEST(JobClientUtilsTest, CreateJobMessageBody) {
  auto json_string = "{\"JobId\":\"6677\",\"ServerJobId\":\"90000\"}";
  auto job_message_body = JobMessageBody(json_string);

  EXPECT_EQ(job_message_body.job_id, "6677");
  EXPECT_EQ(job_message_body.server_job_id, "90000");
}

TEST(JobClientUtilsTest, ConvertJobMessageBodyToJsonString) {
  JobMessageBody job_message_body = JobMessageBody("1234", "55678");
  auto json_string = job_message_body.ToJsonString();

  EXPECT_EQ(json_string, "{\"JobId\":\"1234\",\"ServerJobId\":\"55678\"}");
}

TEST(JobClientUtilsTest, CreateJob) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto updated_time = current_time + TimeUtil::SecondsToDuration(5);
  auto job_body = CreateHelloWorldProtoAsJsonString();
  auto job_status = JobStatus::JOB_STATUS_CREATED;
  auto retry_count = 3;
  auto processing_started_time = current_time + TimeUtil::SecondsToDuration(10);

  auto job = JobClientUtils::CreateJob(kJobId, kServerJobId, job_body,
                                       job_status, current_time, updated_time,
                                       processing_started_time, retry_count);

  Job expected_job;
  expected_job.set_job_id(kJobId);
  expected_job.set_server_job_id(kServerJobId);
  expected_job.set_job_status(job_status);
  *expected_job.mutable_job_body() = job_body;
  *expected_job.mutable_created_time() = current_time;
  *expected_job.mutable_updated_time() = updated_time;
  expected_job.set_retry_count(retry_count);
  *expected_job.mutable_processing_started_time() = processing_started_time;

  EXPECT_THAT(job, EqualsProto(expected_job));
}

TEST(JobClientUtilsTest, ConvertAnyToBase64String) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto helloworld = CreateHelloWorldProtoAsAny(current_time);
  auto string_or = JobClientUtils::ConvertAnyToBase64String(helloworld);
  EXPECT_SUCCESS(string_or);

  std::string decoded_string;
  Base64Decode(*string_or, decoded_string);
  Any any_output;
  any_output.ParseFromString(decoded_string);
  HelloWorld hello_world_output;
  any_output.UnpackTo(&hello_world_output);
  EXPECT_EQ(hello_world_output.name(), kHelloWorldName);
  EXPECT_EQ(hello_world_output.id(), kHelloWorldId);
  EXPECT_EQ(hello_world_output.created_time(), current_time);
}

TEST(JobClientUtilsTest, ConvertBase64StringToAny) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto helloworld = CreateHelloWorldProtoAsAny(current_time);
  std::string string_input;
  helloworld.SerializeToString(&string_input);
  std::string encoded_string;
  Base64Encode(string_input, encoded_string);
  auto any_or = JobClientUtils::ConvertBase64StringToAny(encoded_string);
  EXPECT_SUCCESS(any_or);

  HelloWorld hello_world_output;
  any_or->UnpackTo(&hello_world_output);
  EXPECT_EQ(hello_world_output.name(), kHelloWorldName);
  EXPECT_EQ(hello_world_output.id(), kHelloWorldId);
  EXPECT_EQ(hello_world_output.created_time(), current_time);
}

TEST(JobClientUtilsTest, ConvertDatabaseItemToJob) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto job_body = CreateHelloWorldProtoAsJsonString();
  auto job_status = JobStatus::JOB_STATUS_PROCESSING;
  auto updated_time = current_time;
  auto retry_count = 4;
  auto processing_started_time = current_time + TimeUtil::SecondsToDuration(10);
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(
      CreateJobAsDatabaseItem(job_body, job_status, current_time, updated_time,
                              retry_count, processing_started_time));

  EXPECT_SUCCESS(job_or);

  Job expected_job;
  expected_job.set_job_id(kJobId);
  expected_job.set_server_job_id(kServerJobId);
  expected_job.set_job_status(job_status);
  *expected_job.mutable_job_body() = job_body;
  *expected_job.mutable_created_time() = current_time;
  *expected_job.mutable_updated_time() = updated_time;
  *expected_job.mutable_processing_started_time() = processing_started_time;
  expected_job.set_retry_count(retry_count);

  EXPECT_THAT(*job_or, EqualsProto(expected_job));
}

TEST(JobClientUtilsTest,
     ConvertDatabaseItemToJobWithAttributesInRandomOrderSuccess) {
  Item item;
  *item.mutable_key()->mutable_partition_key() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kJobsTablePartitionKeyName, kJobId);

  auto current_time = TimeUtil::GetCurrentTime();
  auto retry_count = 0;
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeIntAttribute(
          kJobStatusColumnName, JobStatus::JOB_STATUS_PROCESSING);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kCreatedTimeColumnName, TimeUtil::ToString(current_time));
  auto job_body = CreateHelloWorldProtoAsJsonString();
  std::string encoded_job_body;
  Base64Encode(job_body, encoded_job_body);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kJobBodyColumnName, encoded_job_body);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kUpdatedTimeColumnName, TimeUtil::ToString(current_time));
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kProcessingStartedTimeColumnName, TimeUtil::ToString(current_time));
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeIntAttribute(
          kRetryCountColumnName, retry_count);
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          kServerJobIdColumnName, kServerJobId);

  EXPECT_SUCCESS(JobClientUtils::ConvertDatabaseItemToJob(item));
}

TEST(JobClientUtilsTest, ConvertDatabaseItemToJobWithValidationFailure) {
  Item item;
  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);

  EXPECT_THAT(job_or, ResultIs(FailureExecutionResult(
                          SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM)));
}

TEST(JobClientUtilsTest,
     ConvertDatabaseItemToJobWithColumnNamesMismatchFailure) {
  Item item;
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          "invalid_column_name1", "test");
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          "invalid_column_name2", "test");
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          "invalid_column_name3", "test");
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          "invalid_column_name4", "test");
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          "invalid_column_name5", "test");
  *item.add_attributes() =
      google::scp::cpio::client_providers::JobClientUtils::MakeStringAttribute(
          "invalid_column_name6", "test");

  auto job_or = JobClientUtils::ConvertDatabaseItemToJob(item);

  EXPECT_THAT(job_or.result(), ResultIs(FailureExecutionResult(
                                   SC_JOB_CLIENT_PROVIDER_INVALID_JOB_ITEM)));
}

TEST(JobClientUtilsTest, CreateUpsertJobRequest) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto job_body = CreateHelloWorldProtoAsJsonString();
  auto job_status = JobStatus::JOB_STATUS_PROCESSING;
  auto updated_time = current_time;
  auto retry_count = 2;
  auto processing_started_time = current_time + TimeUtil::SecondsToDuration(10);
  auto job = JobClientUtils::CreateJob(kJobId, kServerJobId, job_body,
                                       job_status, current_time, updated_time,
                                       processing_started_time, retry_count);

  auto request_or = JobClientUtils::CreateUpsertJobRequest(kJobsTableName, job);
  EXPECT_SUCCESS(request_or);

  UpsertDatabaseItemRequest expected_request;
  expected_request.mutable_key()->set_table_name(kJobsTableName);
  *expected_request.mutable_key()->mutable_partition_key() =
      JobClientUtils::MakeStringAttribute(kJobsTablePartitionKeyName, kJobId);
  *expected_request.add_new_attributes() =
      JobClientUtils::MakeStringAttribute(kServerJobIdColumnName, kServerJobId);
  std::string encoded_job_body;
  Base64Encode(job_body, encoded_job_body);
  *expected_request.add_new_attributes() =
      JobClientUtils::MakeStringAttribute(kJobBodyColumnName, encoded_job_body);
  *expected_request.add_new_attributes() =
      JobClientUtils::MakeIntAttribute(kJobStatusColumnName, job_status);
  *expected_request.add_new_attributes() = JobClientUtils::MakeStringAttribute(
      kCreatedTimeColumnName, TimeUtil::ToString(current_time));
  *expected_request.add_new_attributes() = JobClientUtils::MakeStringAttribute(
      kUpdatedTimeColumnName, TimeUtil::ToString(updated_time));
  *expected_request.add_new_attributes() =
      JobClientUtils::MakeIntAttribute(kRetryCountColumnName, retry_count);
  *expected_request.add_new_attributes() = JobClientUtils::MakeStringAttribute(
      kProcessingStartedTimeColumnName,
      TimeUtil::ToString(processing_started_time));

  EXPECT_THAT(*request_or, EqualsProto(expected_request));
}

TEST(JobClientUtilsTest, CreateUpsertJobRequestWithPartialUpdate) {
  Job job;
  job.set_job_id(kJobId);
  auto job_status = JobStatus::JOB_STATUS_PROCESSING;
  job.set_job_status(job_status);
  auto updated_time = TimeUtil::GetCurrentTime();
  *job.mutable_updated_time() = updated_time;

  auto request_or = JobClientUtils::CreateUpsertJobRequest(kJobsTableName, job);
  EXPECT_SUCCESS(request_or);

  UpsertDatabaseItemRequest expected_request;
  expected_request.mutable_key()->set_table_name(kJobsTableName);
  *expected_request.mutable_key()->mutable_partition_key() =
      JobClientUtils::MakeStringAttribute(kJobsTablePartitionKeyName, kJobId);
  *expected_request.add_new_attributes() =
      JobClientUtils::MakeIntAttribute(kJobStatusColumnName, job_status);
  *expected_request.add_new_attributes() = JobClientUtils::MakeStringAttribute(
      kUpdatedTimeColumnName, TimeUtil::ToString(updated_time));
  *expected_request.add_new_attributes() =
      JobClientUtils::MakeIntAttribute(kRetryCountColumnName, 0);

  EXPECT_THAT(*request_or, EqualsProto(expected_request));
}

TEST(JobClientUtilsTest, CreatePutJobRequest) {
  auto current_time = TimeUtil::GetCurrentTime();
  auto job_body = CreateHelloWorldProtoAsJsonString();
  auto job_status = JobStatus::JOB_STATUS_PROCESSING;
  auto updated_time = current_time;
  auto retry_count = 2;
  auto processing_started_time = TimeUtil::SecondsToTimestamp(0);
  auto job = JobClientUtils::CreateJob(kJobId, kServerJobId, job_body,
                                       job_status, current_time, updated_time,
                                       processing_started_time, retry_count);

  auto request_or = JobClientUtils::CreatePutJobRequest(kJobsTableName, job);
  EXPECT_SUCCESS(request_or);

  CreateDatabaseItemRequest expected_request;
  expected_request.mutable_key()->set_table_name(kJobsTableName);
  *expected_request.mutable_key()->mutable_partition_key() =
      JobClientUtils::MakeStringAttribute(kJobsTablePartitionKeyName, kJobId);
  *expected_request.add_attributes() =
      JobClientUtils::MakeStringAttribute(kServerJobIdColumnName, kServerJobId);
  std::string encoded_job_body;
  Base64Encode(job_body, encoded_job_body);
  *expected_request.add_attributes() =
      JobClientUtils::MakeStringAttribute(kJobBodyColumnName, encoded_job_body);
  *expected_request.add_attributes() =
      JobClientUtils::MakeIntAttribute(kJobStatusColumnName, job_status);
  *expected_request.add_attributes() = JobClientUtils::MakeStringAttribute(
      kCreatedTimeColumnName, TimeUtil::ToString(current_time));
  *expected_request.add_attributes() = JobClientUtils::MakeStringAttribute(
      kUpdatedTimeColumnName, TimeUtil::ToString(updated_time));
  *expected_request.add_attributes() =
      JobClientUtils::MakeIntAttribute(kRetryCountColumnName, retry_count);
  *expected_request.add_attributes() = JobClientUtils::MakeStringAttribute(
      kProcessingStartedTimeColumnName,
      TimeUtil::ToString(processing_started_time));
  EXPECT_THAT(*request_or, EqualsProto(expected_request));
}

TEST(JobClientUtilsTest, CreateGetNextJobRequest) {
  auto request = JobClientUtils::CreateGetNextJobRequest(kJobsTableName, kJobId,
                                                         kServerJobId);

  GetDatabaseItemRequest expected_request;
  expected_request.mutable_key()->set_table_name(kJobsTableName);
  *expected_request.mutable_key()->mutable_partition_key() =
      JobClientUtils::MakeStringAttribute(kJobsTablePartitionKeyName, kJobId);
  *expected_request.add_required_attributes() =
      JobClientUtils::MakeStringAttribute(kServerJobIdColumnName, kServerJobId);

  EXPECT_THAT(*request, EqualsProto(expected_request));
}

TEST(JobClientUtilsTest, CreateGetJobByJobIdRequest) {
  auto request =
      JobClientUtils::CreateGetJobByJobIdRequest(kJobsTableName, kJobId);

  GetDatabaseItemRequest expected_request;
  expected_request.mutable_key()->set_table_name(kJobsTableName);
  *expected_request.mutable_key()->mutable_partition_key() =
      JobClientUtils::MakeStringAttribute(kJobsTablePartitionKeyName, kJobId);

  EXPECT_THAT(*request, EqualsProto(expected_request));
}

INSTANTIATE_TEST_SUITE_P(
    CompletedJobStatus, JobClientUtilsTest,
    testing::Values(
        std::make_tuple(JobStatus::JOB_STATUS_CREATED,
                        JobStatus::JOB_STATUS_PROCESSING,
                        SuccessExecutionResult()),
        std::make_tuple(JobStatus::JOB_STATUS_CREATED,
                        JobStatus::JOB_STATUS_SUCCESS,
                        SuccessExecutionResult()),
        std::make_tuple(JobStatus::JOB_STATUS_CREATED,
                        JobStatus::JOB_STATUS_FAILURE,
                        SuccessExecutionResult()),
        std::make_tuple(JobStatus::JOB_STATUS_PROCESSING,
                        JobStatus::JOB_STATUS_PROCESSING,
                        SuccessExecutionResult()),
        std::make_tuple(JobStatus::JOB_STATUS_PROCESSING,
                        JobStatus::JOB_STATUS_SUCCESS,
                        SuccessExecutionResult()),
        std::make_tuple(JobStatus::JOB_STATUS_PROCESSING,
                        JobStatus::JOB_STATUS_FAILURE,
                        SuccessExecutionResult()),
        std::make_tuple(
            JobStatus::JOB_STATUS_SUCCESS, JobStatus::JOB_STATUS_PROCESSING,
            FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS)),
        std::make_tuple(
            JobStatus::JOB_STATUS_FAILURE, JobStatus::JOB_STATUS_PROCESSING,
            FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS)),
        std::make_tuple(
            JobStatus::JOB_STATUS_CREATED, JobStatus::JOB_STATUS_UNKNOWN,
            FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS)),
        std::make_tuple(
            JobStatus::JOB_STATUS_PROCESSING, JobStatus::JOB_STATUS_CREATED,
            FailureExecutionResult(SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS)),
        std::make_tuple(JobStatus::JOB_STATUS_PROCESSING,
                        JobStatus::JOB_STATUS_UNKNOWN,
                        FailureExecutionResult(
                            SC_JOB_CLIENT_PROVIDER_INVALID_JOB_STATUS))));

TEST_P(JobClientUtilsTest, ValidateJobStatus) {
  EXPECT_THAT(
      JobClientUtils::ValidateJobStatus(GetCurrentStatus(), GetUpdateStatus()),
      ResultIs(GetExpectedExecutionResult()));
}

}  // namespace google::scp::cpio::client_providers::test
