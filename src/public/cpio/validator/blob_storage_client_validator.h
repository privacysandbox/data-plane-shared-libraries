/*
 * Copyright 2023 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef PUBLIC_CPIO_VALIDATOR_BLOB_STORAGE_CLIENT_VALIDATOR_H_
#define PUBLIC_CPIO_VALIDATOR_BLOB_STORAGE_CLIENT_VALIDATOR_H_

#include "src/public/cpio/interface/blob_storage_client/blob_storage_client_interface.h"
#include "src/public/cpio/validator/proto/validator_config.pb.h"

namespace google::scp::cpio::validator {

void RunGetBlobValidator(
    std::string_view name,
    const google::scp::cpio::validator::proto::GetBlobConfig& get_blob_config);

void RunListBlobsMetadataValidator(
    std::string_view name,
    const google::scp::cpio::validator::proto::ListBlobsMetadataConfig&
        list_blobs_metadata_config);

};  // namespace google::scp::cpio::validator
#endif  // PUBLIC_CPIO_VALIDATOR_BLOB_STORAGE_CLIENT_VALIDATOR_H_
