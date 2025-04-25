
/*
 * Copyright 2024 Google LLC
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

#ifndef PARC_UTILS_STATUS_UTIL_H_
#define PARC_UTILS_STATUS_UTIL_H_

#include "absl/status/status.h"
#include "include/grpcpp/grpcpp.h"
#include "src/proto/grpc/status/status.pb.h"

namespace privacysandbox::parc::utils {

// Converts from absl::Status to grpc::Status.
grpc::Status FromAbslStatus(const absl::Status& status);

// Converts from grpc::Status to absl::Status.
absl::Status ToAbslStatus(const grpc::Status& status);

}  // namespace privacysandbox::parc::utils

#endif  // PARC_UTILS_STATUS_UTIL_H_
