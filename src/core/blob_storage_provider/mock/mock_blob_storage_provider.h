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

#ifndef CORE_BLOB_STORAGE_PROVIDER_MOCK_MOCK_BLOB_STORAGE_PROVIDER_H_
#define CORE_BLOB_STORAGE_PROVIDER_MOCK_MOCK_BLOB_STORAGE_PROVIDER_H_

#include <algorithm>
#include <bitset>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/strings/strip.h"
#include "src/core/blob_storage_provider/common/error_codes.h"
#include "src/core/interface/blob_storage_provider_interface.h"

namespace google::scp::core::blob_storage_provider::mock {

inline bool CompareBlobs(Blob& lblob, Blob& rblob) {
  return *lblob.blob_name < *rblob.blob_name;
}

class MockBlobStorageClient : public BlobStorageClientInterface {
 public:
  ExecutionResult GetBlob(AsyncContext<GetBlobRequest, GetBlobResponse>&
                              get_blob_context) noexcept {
    if (get_blob_mock) {
      return get_blob_mock(get_blob_context);
    }
    std::string full_path =
        absl::StrCat(*get_blob_context.request->bucket_name, "/",
                     *get_blob_context.request->blob_name);

    std::ifstream input_stream(full_path, std::ios::binary | std::ios::ate);

    if (!input_stream) {
      get_blob_context.Finish(FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND));
      return SuccessExecutionResult();
    }

    auto end_offset = input_stream.tellg();
    input_stream.seekg(0, std::ios::beg);

    auto content_length = std::size_t(end_offset - input_stream.tellg());
    get_blob_context.response = std::make_shared<GetBlobResponse>();
    get_blob_context.response->buffer = std::make_shared<BytesBuffer>();
    get_blob_context.response->buffer->length = content_length;
    get_blob_context.response->buffer->capacity = content_length;

    if (content_length != 0) {
      get_blob_context.response->buffer->bytes =
          std::make_shared<std::vector<Byte>>(content_length);

      if (!input_stream.read(
              reinterpret_cast<char*>(
                  get_blob_context.response->buffer->bytes->data()),
              content_length)) {
        get_blob_context.Finish(FailureExecutionResult(
            errors::SC_BLOB_STORAGE_PROVIDER_ERROR_GETTING_BLOB));
        return SuccessExecutionResult();
      }
    }

    get_blob_context.Finish(SuccessExecutionResult());
    return SuccessExecutionResult();
  }

  ExecutionResult ListBlobs(AsyncContext<ListBlobsRequest, ListBlobsResponse>&
                                list_blobs_context) noexcept {
    if (list_blobs_mock) {
      return list_blobs_mock(list_blobs_context);
    }
    auto directory_path = *list_blobs_context.request->bucket_name;

    list_blobs_context.response = std::make_shared<ListBlobsResponse>();
    list_blobs_context.response->blobs = std::make_shared<std::vector<Blob>>();
    auto& blob_name_prefix = list_blobs_context.request->blob_name;

    for (const auto& dir_entry :
         std::filesystem::recursive_directory_iterator(directory_path)) {
      std::string_view blob_name(dir_entry.path().c_str());
      absl::ConsumePrefix(&blob_name,
                          *list_blobs_context.request->bucket_name + "/");
      if (blob_name_prefix && !blob_name_prefix->empty() &&
          (std::mismatch(blob_name_prefix->begin(), blob_name_prefix->end(),
                         blob_name.begin())
               .first != blob_name_prefix->end())) {
        // Prefix mismatch, skip this entry.
        continue;
      }
      Blob blob;
      blob.blob_name = std::make_shared<std::string>(blob_name);
      list_blobs_context.response->blobs->push_back(std::move(blob));
    }

    auto& blobs_in_response = list_blobs_context.response->blobs;
    auto& supplied_marker = list_blobs_context.request->marker;

    std::sort(blobs_in_response->begin(), blobs_in_response->end(),
              CompareBlobs);

    // Filter based on supplied marker
    if (supplied_marker && !supplied_marker->empty()) {
      for (auto it = blobs_in_response->begin();
           it != blobs_in_response->end();) {
        if (supplied_marker->compare(*it->blob_name) >= 0) {
          // Remove all blobs that are less or equal to the marker name
          it = blobs_in_response->erase(it);
        } else {
          it++;
        }
      }
    }

    // Populate next marker, with the last blob name in the response list of
    // blobs.
    if (!blobs_in_response->empty()) {
      auto next_marker = std::make_shared<Blob>();
      next_marker->bucket_name =
          list_blobs_context.response->blobs->rbegin()->bucket_name;
      next_marker->blob_name =
          list_blobs_context.response->blobs->rbegin()->blob_name;
      list_blobs_context.response->next_marker = next_marker;
    }

    list_blobs_context.Finish(SuccessExecutionResult());
    return SuccessExecutionResult();
  }

  ExecutionResult PutBlob(AsyncContext<PutBlobRequest, PutBlobResponse>&
                              put_blob_context) noexcept {
    if (put_blob_mock) {
      return put_blob_mock(put_blob_context);
    }
    std::string full_path =
        absl::StrCat(*put_blob_context.request->bucket_name, "/",
                     *put_blob_context.request->blob_name);

    std::filesystem::path storage_path(full_path);
    std::filesystem::create_directories(storage_path.parent_path());

    std::ofstream output_stream(full_path, std::ofstream::trunc);
    output_stream.write(reinterpret_cast<char*>(
                            put_blob_context.request->buffer->bytes->data()),
                        put_blob_context.request->buffer->length);
    output_stream.close();

    put_blob_context.Finish(SuccessExecutionResult());
    return SuccessExecutionResult();
  }

  ExecutionResult DeleteBlob(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&
          delete_blob_context) noexcept {
    if (delete_blob_mock) {
      return delete_blob_mock(delete_blob_context);
    }
    std::string full_path =
        absl::StrCat(*delete_blob_context.request->bucket_name, "/",
                     *delete_blob_context.request->blob_name);
    if (std::error_code ec; std::filesystem::remove_all(full_path, ec) ==
                            static_cast<std::uintmax_t>(-1)) {
      delete_blob_context.Finish(FailureExecutionResult(
          errors::SC_BLOB_STORAGE_PROVIDER_BLOB_PATH_NOT_FOUND));
      return SuccessExecutionResult();
    }

    delete_blob_context.Finish(SuccessExecutionResult());
    return SuccessExecutionResult();
  }

  std::function<ExecutionResult(AsyncContext<GetBlobRequest, GetBlobResponse>&)>
      get_blob_mock;
  std::function<ExecutionResult(
      AsyncContext<ListBlobsRequest, ListBlobsResponse>&)>
      list_blobs_mock;
  std::function<ExecutionResult(AsyncContext<PutBlobRequest, PutBlobResponse>&)>
      put_blob_mock;
  std::function<ExecutionResult(
      AsyncContext<DeleteBlobRequest, DeleteBlobResponse>&)>
      delete_blob_mock;
};

class MockBlobStorageProvider : public BlobStorageProviderInterface {
 public:
  ExecutionResult Init() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Run() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult Stop() noexcept override { return SuccessExecutionResult(); };

  ExecutionResult CreateBlobStorageClient(
      std::shared_ptr<BlobStorageClientInterface>& blob_storage_client) noexcept
      override {
    blob_storage_client = std::make_shared<MockBlobStorageClient>();
    return SuccessExecutionResult();
  }
};
}  // namespace google::scp::core::blob_storage_provider::mock

#endif  // CORE_BLOB_STORAGE_PROVIDER_MOCK_MOCK_BLOB_STORAGE_PROVIDER_H_
