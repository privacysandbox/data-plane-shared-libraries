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

#include "aws_helper.h"

#include <chrono>
#include <memory>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include <aws/core/client/ClientConfiguration.h>
#include <aws/dynamodb/DynamoDBClient.h>
#include <aws/dynamodb/model/CreateTableRequest.h>
#include <aws/kms/KMSClient.h>
#include <aws/kms/model/CreateKeyRequest.h>
#include <aws/kms/model/EncryptRequest.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CreateBucketRequest.h>
#include <aws/ssm/SSMClient.h>
#include <aws/ssm/model/GetParameterRequest.h>
#include <aws/ssm/model/PutParameterRequest.h>

using Aws::Client::ClientConfiguration;
using Aws::DynamoDB::DynamoDBClient;
using Aws::DynamoDB::DynamoDBErrors;
using Aws::DynamoDB::Model::AttributeDefinition;
using Aws::DynamoDB::Model::CreateTableOutcome;
using Aws::DynamoDB::Model::CreateTableRequest;
using Aws::DynamoDB::Model::KeySchemaElement;
using Aws::DynamoDB::Model::KeyType;
using Aws::DynamoDB::Model::ProvisionedThroughput;
using Aws::DynamoDB::Model::ScalarAttributeType;
using Aws::KMS::KMSClient;
using Aws::KMS::Model::CreateKeyRequest;
using Aws::KMS::Model::EncryptRequest;
using Aws::S3::S3Client;
using Aws::S3::Model::BucketCannedACL;
using Aws::S3::Model::CreateBucketRequest;
using Aws::SSM::SSMClient;
using Aws::SSM::Model::GetParameterRequest;
using Aws::SSM::Model::PutParameterRequest;
using Aws::Utils::CryptoBuffer;

/// Fixed connect timeout to create an AWS client.
constexpr int kConnectTimeoutMs = 6000;
/// Fixed request timeout to create an AWS client.
constexpr int kRequestTimeoutMs = 12000;
/// Fixed read and write capacity for DynamoDB.
constexpr int kReadWriteCapacity = 10;

namespace google::scp::core::test {
std::shared_ptr<ClientConfiguration> CreateClientConfiguration(
    std::string_view endpoint, std::string_view region) {
  auto config = std::make_shared<ClientConfiguration>();
  config->region = region;
  config->scheme = Aws::Http::Scheme::HTTP;
  config->connectTimeoutMs = kConnectTimeoutMs;
  config->requestTimeoutMs = kRequestTimeoutMs;
  config->endpointOverride = endpoint;
  return config;
}

std::shared_ptr<DynamoDBClient> CreateDynamoDbClient(std::string_view endpoint,
                                                     std::string_view region) {
  return std::make_shared<DynamoDBClient>(
      *CreateClientConfiguration(endpoint, region));
}

void CreateTable(const std::shared_ptr<DynamoDBClient>& dynamo_db_client,
                 std::string_view table_name,
                 const std::vector<AttributeDefinition>& attributes,
                 const std::vector<KeySchemaElement>& schemas) {
  CreateTableRequest request;
  request.SetTableName(table_name.data());

  for (auto attribute : attributes) {
    request.AddAttributeDefinitions(attribute);
  }

  for (auto schema : schemas) {
    request.AddKeySchema(schema);
  }

  ProvisionedThroughput throughput;
  throughput.SetReadCapacityUnits(kReadWriteCapacity);
  throughput.SetWriteCapacityUnits(kReadWriteCapacity);
  request.SetProvisionedThroughput(throughput);

  auto outcome = dynamo_db_client->CreateTable(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to create table: " << outcome.GetError().GetMessage()
              << std::endl;
  } else {
    std::cout << "Succeeded to create table:" << table_name << std::endl;
  }
}

std::shared_ptr<S3Client> CreateS3Client(std::string_view endpoint,
                                         std::string_view region) {
  // Should disable virtual host, otherwise, our path-style url will not work.
  return std::make_shared<S3Client>(
      *CreateClientConfiguration(endpoint, region),
      Aws::Client::AWSAuthV4Signer::PayloadSigningPolicy::Never,
      /* use virtual host */ false);
}

void CreateBucket(const std::shared_ptr<S3Client>& s3_client,
                  std::string_view bucket_name) {
  CreateBucketRequest request;
  request.SetBucket(bucket_name.data());
  request.SetACL(BucketCannedACL::public_read_write);

  auto outcome = s3_client->CreateBucket(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to create bucket: " << outcome.GetError().GetMessage()
              << std::endl;
  } else {
    std::cout << "Succeeded to create bucket: " << bucket_name << std::endl;
  }
}

std::shared_ptr<SSMClient> CreateSSMClient(std::string_view endpoint,
                                           std::string_view region) {
  return std::make_shared<SSMClient>(
      *CreateClientConfiguration(endpoint, region));
}

void PutParameter(const std::shared_ptr<SSMClient>& ssm_client,
                  std::string_view parameter_name,
                  std::string_view parameter_value) {
  PutParameterRequest request;
  request.SetName(parameter_name.data());
  request.SetValue(parameter_value.data());

  auto outcome = ssm_client->PutParameter(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to put parameter: " << outcome.GetError().GetMessage()
              << std::endl;
  } else {
    std::cout << "Succeeded to put parameter:" << parameter_name << std::endl;
  }
}

std::string GetParameter(const std::shared_ptr<SSMClient>& ssm_client,
                         std::string_view parameter_name) {
  GetParameterRequest request;
  request.SetName(parameter_name.data());

  auto outcome = ssm_client->GetParameter(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to get parameter: " << outcome.GetError().GetMessage()
              << std::endl;
    return "";
  }

  std::cout << "Succeeded to get parameter:" << parameter_name << std::endl;
  return outcome.GetResult().GetParameter().GetValue();
}

std::shared_ptr<KMSClient> CreateKMSClient(std::string_view endpoint,
                                           std::string_view region) {
  return std::make_shared<KMSClient>(
      *CreateClientConfiguration(endpoint, region));
}

void CreateKey(const std::shared_ptr<KMSClient>& kms_client,
               std::string& key_id, std::string& key_resource_name) {
  CreateKeyRequest request;

  // Needs to retry until succeeded.
  int8_t retry_count = 0;
  while (retry_count < 20) {
    auto outcome = kms_client->CreateKey(request);
    if (!outcome.IsSuccess()) {
      std::cout << "Failed to create key: " << outcome.GetError().GetMessage()
                << std::endl;
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      ++retry_count;
    } else {
      std::cout << "Succeeded to create key." << std::endl;
      key_id = outcome.GetResult().GetKeyMetadata().GetKeyId();
      key_resource_name = outcome.GetResult().GetKeyMetadata().GetArn();
      return;
    }
  }
}

std::string Encrypt(const std::shared_ptr<KMSClient>& kms_client,
                    std::string_view key_id, std::string_view plaintext) {
  EncryptRequest request;
  request.SetKeyId(key_id.data());
  Aws::Utils::ByteBuffer plaintext_buffer(
      reinterpret_cast<const unsigned char*>(plaintext.data()),
      plaintext.length());
  request.SetPlaintext(std::move(plaintext_buffer));
  auto outcome = kms_client->Encrypt(request);
  if (!outcome.IsSuccess()) {
    std::cout << "Failed to encrypt: " << outcome.GetError().GetMessage()
              << std::endl;
    return "";
  } else {
    std::cout << "Succeeded to encrypt: " << plaintext << std::endl;
    auto& blob = outcome.GetResult().GetCiphertextBlob();
    return std::string(reinterpret_cast<const char*>(blob.GetUnderlyingData()),
                       blob.GetLength());
  }
}
}  // namespace google::scp::core::test
