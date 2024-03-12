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

#ifndef SCP_CPIO_INTERFACE_TYPE_DEFS_H_
#define SCP_CPIO_INTERFACE_TYPE_DEFS_H_

#include <functional>
#include <memory>
#include <string>

#include "src/core/common/global_logger/global_logger.h"
#include "src/core/interface/async_executor_interface.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::cpio {
using AccountIdentity = std::string;
using Region = std::string;
using PublicKeyValue = std::string;
using PublicPrivateKeyPairId = std::string;
using Timestamp = int64_t;
using core::common::LogOption;

/// Option for whether to initialize cloud in Cpio. Customers can configure it
/// in case they need to do initialization and shutdown in their side. In AWS,
/// Cpio will call Aws::InitAPI and Aws::ShutdownAPI. In GCP, no init or
/// shutdown are needed.
enum class CloudInitOption {
  /// Doesn't initialize cloud in CPIO.
  kNoInitInCpio = 1,
  /// Initializes cloud in CPIO.
  kInitInCpio = 2,
};

/// Global options for CPIO.
struct CpioOptions {
  /// Default is kNoLog.
  LogOption log_option = LogOption::kNoLog;

  /// Default is kInitInCpio.
  CloudInitOption cloud_init_option = CloudInitOption::kInitInCpio;

  /// Project ID for GCP. If set at neither the CPIO nor service level, found
  /// through the instance client.
  /// Implemented for Blob Storage and Parameter clients.
  std::string project_id;

  /// Location ID for GCP, region code for AWS. If set at neither the CPIO nor
  /// service level, found through the instance client.
  /// Implemented for Blob Storage, Metric, and Parameter clients.
  std::string region;
};

template <typename TResponse>
using Callback =
    typename std::function<void(const core::ExecutionResult&, TResponse)>;
}  // namespace google::scp::cpio

#endif  // SCP_CPIO_INTERFACE_TYPE_DEFS_H_
