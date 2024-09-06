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

#ifndef CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_AWS_AWS_BLOB_STORAGE_CLIENT_PROVIDER_H_
#define CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_AWS_AWS_BLOB_STORAGE_CLIENT_PROVIDER_H_

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include <aws/core/Aws.h>
#include <aws/s3/S3Client.h>
#include <aws/s3/model/CompleteMultipartUploadRequest.h>
#include <aws/s3/model/CompletedMultipartUpload.h>
#include <aws/s3/model/CreateMultipartUploadRequest.h>
#include <aws/s3/model/GetObjectRequest.h>
#include <aws/s3/model/ListObjectsRequest.h>

#include "absl/base/nullability.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/core/interface/config_provider_interface.h"
#include "src/core/interface/streaming_context.h"
#include "src/cpio/client_providers/interface/blob_storage_client_provider_interface.h"
#include "src/cpio/client_providers/interface/instance_client_provider_interface.h"
#include "src/public/cpio/interface/blob_storage_client/type_def.h"

namespace google::scp::cpio::client_providers {

class AwsS3Factory;

/*! @copydoc BlobStorageClientProviderInterface
 */
class AwsBlobStorageClientProvider : public BlobStorageClientProviderInterface {
 public:
  explicit AwsBlobStorageClientProvider(
      BlobStorageClientOptions options,
      absl::Nonnull<InstanceClientProviderInterface*> instance_client,
      absl::Nonnull<core::AsyncExecutorInterface*> cpu_async_executor,
      absl::Nonnull<core::AsyncExecutorInterface*> io_async_executor,
      absl::Nonnull<std::unique_ptr<AwsS3Factory>> s3_factory =
          std::make_unique<AwsS3Factory>())
      : instance_client_(instance_client),
        cpu_async_executor_(cpu_async_executor),
        io_async_executor_(io_async_executor),
        region_code_(std::move(options).region),
        s3_factory_(std::move(s3_factory)) {}

  absl::Status Init() noexcept;

  absl::Status GetBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&
          get_blob_context) noexcept override;

  absl::Status GetBlobStream(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          get_blob_stream_context) noexcept override;

  absl::Status ListBlobsMetadata(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&
          list_blobs_metadata_context) noexcept override;

  absl::Status PutBlob(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&
          put_blob_context) noexcept override;

  absl::Status PutBlobStream(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context) noexcept override;

  absl::Status DeleteBlob(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&
          delete_blob_context) noexcept override;

 private:
  /**
   * @brief Is called when the object is returned from the S3 GetObject
   * callback.
   *
   * @param get_blob_context The get blob context object.
   * @param s3_client An instance of the S3 client.
   * @param get_object_request The get object request.
   * @param get_object_outcome The get object outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnGetObjectCallback(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::GetBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::GetBlobResponse>&
          get_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::GetObjectRequest& get_object_request,
      Aws::S3::Model::GetObjectOutcome get_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  struct GetBlobStreamTracker {
    // What byte indices were just used.
    int64_t last_begin_byte_index, last_end_byte_index;
    // How many bytes remain in the object.
    int64_t bytes_remaining;
    // How many bytes (maximum) should be placed in each GetBlobStreamResponse.
    int64_t max_bytes_per_response;
  };

  /**
   * @brief Is called when a partial GetObject call is done.
   *
   * @param get_blob_stream_context The get blob stream context object.
   * @param s3_client An instance of the S3 client.
   * @param get_object_request The get object request.
   * @param get_object_outcome The get object outcome
   * of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnGetObjectStreamCallback(
      core::ConsumerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::GetBlobStreamResponse>&
          get_blob_stream_context,
      std::shared_ptr<GetBlobStreamTracker> tracker,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::GetObjectRequest& get_object_request,
      Aws::S3::Model::GetObjectOutcome get_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when objects are list and returned from the S3 ListObjects
   * callback.
   *
   * @param list_blobs_metadata_context The list blobs metadata context object.
   * @param s3_client An instance of the S3 client.
   * @param list_object_request The list objects request.
   * @param list_object_outcome The list objects outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnListObjectsMetadataCallback(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataRequest,
          cmrt::sdk::blob_storage_service::v1::ListBlobsMetadataResponse>&
          list_blobs_metadata_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::ListObjectsRequest& list_objects_request,
      Aws::S3::Model::ListObjectsOutcome list_objects_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the S3 PutObject
   * callback.
   *
   * @param put_blob_context The put blob context object.
   * @param s3_client An instance of the S3 client.
   * @param put_object_request The put object request.
   * @param put_object_outcome The put object outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnPutObjectCallback(
      core::AsyncContext<cmrt::sdk::blob_storage_service::v1::PutBlobRequest,
                         cmrt::sdk::blob_storage_service::v1::PutBlobResponse>&
          put_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::PutObjectRequest& put_object_request,
      Aws::S3::Model::PutObjectOutcome put_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  struct PutBlobStreamTracker {
    // The expected bucket and blob name for this upload. If this is different
    // at any point in the upload, the upload fails.
    std::string bucket_name, blob_name;
    // The unique identifier to use for this upload.
    std::string upload_id;
    // The part number to use for the next part.
    int64_t next_part_number = 1;
    // S3 requires each upload except the last must be at least 5MiB.
    // https://docs.aws.amazon.com/AmazonS3/latest/userguide/qfacts.html
    // Any partial data will be stored here.
    std::string accumulated_contents;
    // The object used to complete the upload.
    Aws::S3::Model::CompletedMultipartUpload completed_multipart_upload;

    // Timestamp in nanoseconds of when this PutBlobStream session should
    // expire.
    std::chrono::nanoseconds expiry_time_ns =
        std::chrono::duration<int64_t>::min();
  };

  // Schedules another poll for the next message in PutBlobStream.
  void ScheduleAnotherPutBlobStreamPoll(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      std::shared_ptr<PutBlobStreamTracker> tracker,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::UploadPartRequest& upload_part_request,
      Aws::S3::Model::UploadPartOutcome upload_part_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context,
      std::chrono::seconds rescan_time);

  /**
   * @brief Is called when the multipart upload is created.
   *
   * @param put_blob_stream_context The put blob stream context object.
   * @param s3_client An instance of the S3 client.
   * @param create_multipart_upload_request The create multipart upload request.
   * @param create_multipart_upload_outcome The create multipart upload outcome
   * of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnCreateMultipartUploadCallback(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::CreateMultipartUploadRequest&
          create_multipart_upload_request,
      Aws::S3::Model::CreateMultipartUploadOutcome
          create_multipart_upload_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when an UploadPart is done.
   *
   * @param put_blob_stream_context The put blob stream context object.
   * @param s3_client An instance of the S3 client.
   * @param upload_part_request The upload part request.
   * @param upload_part_outcome The upload part outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnUploadPartCallback(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      std::shared_ptr<PutBlobStreamTracker> tracker,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::UploadPartRequest& upload_part_request,
      Aws::S3::Model::UploadPartOutcome upload_part_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Completes a multipart upload.
   *
   * @param put_blob_stream_context
   * @param tracker
   */
  void CompleteUpload(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      std::shared_ptr<PutBlobStreamTracker> tracker);

  /**
   * @brief Is called when CompleteMultipartUpload is done.
   *
   * @param put_blob_stream_context The put blob stream context object.
   * @param s3_client An instance of the S3 client.
   * @param complete_multipart_upload_request The complete multipart upload
   * request.
   * @param complete_multipart_upload_outcome The complete multipart upload
   * outcome of the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnCompleteMultipartUploadCallback(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::CompleteMultipartUploadRequest&
          complete_multipart_upload_request,
      Aws::S3::Model::CompleteMultipartUploadOutcome
          complete_multipart_upload_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Aborts a multipart upload.
   *
   * @param put_blob_stream_context
   * @param tracker
   */
  void AbortUpload(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      std::shared_ptr<PutBlobStreamTracker> tracker);

  /**
   * @brief Is called when AbortMultipartUpload is done.
   *
   * @param put_blob_stream_context The put blob stream context object.
   * @param s3_client An instance of the S3 client.
   * @param abort_multipart_upload_request The abort multipart upload request.
   * @param abort_multipart_upload_outcome The abort multipart upload outcome of
   * the async operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnAbortMultipartUploadCallback(
      core::ProducerStreamingContext<
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamRequest,
          cmrt::sdk::blob_storage_service::v1::PutBlobStreamResponse>&
          put_blob_stream_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::AbortMultipartUploadRequest&
          abort_multipart_upload_request,
      Aws::S3::Model::AbortMultipartUploadOutcome
          abort_multipart_upload_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Is called when the object is returned from the S3 DeleteObject
   * callback.
   *
   * @param delete_blob_context The delete blob context object.
   * @param s3_client An instance of the S3 client.
   * @param delete_object_request The delete object request.
   * @param delete_object_outcome The delete object outcome of the async
   * operation.
   * @param async_context The Aws async context. This arg is not used.
   */
  void OnDeleteObjectCallback(
      core::AsyncContext<
          cmrt::sdk::blob_storage_service::v1::DeleteBlobRequest,
          cmrt::sdk::blob_storage_service::v1::DeleteBlobResponse>&
          delete_blob_context,
      const Aws::S3::S3Client* s3_client,
      const Aws::S3::Model::DeleteObjectRequest& delete_object_request,
      Aws::S3::Model::DeleteObjectOutcome delete_object_outcome,
      const std::shared_ptr<const Aws::Client::AsyncCallerContext>
          async_context) noexcept;

  /**
   * @brief Creates the Client Config object.
   *
   * @param region the region of the client.
   * @return std::shared_ptr<Aws::Client::ClientConfiguration> client
   * configuration.
   */
  virtual Aws::Client::ClientConfiguration CreateClientConfiguration(
      std::string_view region) noexcept;

  InstanceClientProviderInterface* instance_client_;

  /// Instances of the async executor for local compute and blocking IO
  /// operations respectively.
  core::AsyncExecutorInterface* cpu_async_executor_;
  core::AsyncExecutorInterface* io_async_executor_;
  std::string region_code_;

  // TODO(b/321321138): Rewrite test case to make pointer unnecessary.
  // An instance of the factory for Aws::S3::S3Client.
  std::unique_ptr<AwsS3Factory> s3_factory_;

  /// An instance of the AWS S3 client.
  std::shared_ptr<Aws::S3::S3Client> s3_client_;
};

/// Creates Aws::S3::S3Client
class AwsS3Factory {
 public:
  // TODO(b/321793964): Update to return unique_ptr
  virtual core::ExecutionResultOr<std::shared_ptr<Aws::S3::S3Client>>
  CreateClient(Aws::Client::ClientConfiguration client_config,
               core::AsyncExecutorInterface* async_executor) noexcept;

  virtual ~AwsS3Factory() = default;
};

}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_BLOB_STORAGE_CLIENT_PROVIDER_AWS_AWS_BLOB_STORAGE_CLIENT_PROVIDER_H_
