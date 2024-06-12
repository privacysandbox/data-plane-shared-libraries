/*
 * Copyright 2024 Google LLC
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

#ifndef SRC_ROMA_GVISOR_CONTAINER_SANDBOX_POOL_MANAGER_H_
#define SRC_ROMA_GVISOR_CONTAINER_SANDBOX_POOL_MANAGER_H_

#include <filesystem>
#include <queue>
#include <string>
#include <string_view>
#include <thread>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/types/span.h"
#include "src/roma/gvisor/host/uuid.pb.h"

namespace privacy_sandbox::server_common::gvisor {

struct WorkerInfo {
  pid_t pid;
  int in_pipe;
  int out_pipe;
  std::string pivot_root_dir;
  int comms_fd;
};

class RomaGvisorPoolManager final {
 public:
  explicit RomaGvisorPoolManager(int worker_pool_size,
                                 absl::Span<const std::string> mounts,
                                 std::string_view prog_dir,
                                 std::string_view callback_socket);

  ~RomaGvisorPoolManager();

  // Loads new binary. Version string is used as a key -- just like it is for
  // Roma V8.
  absl::StatusOr<std::string> LoadBinary(std::string_view version,
                                         std::string_view code)
      ABSL_LOCKS_EXCLUDED(worker_map_mu_);

  absl::StatusOr<absl::Cord> SendRequestAndGetResponseFromWorker(
      std::string_view request_id, std::string_view code_token,
      std::string_view serialized_bin_request)
      ABSL_LOCKS_EXCLUDED(worker_map_mu_);

 private:
  int worker_pool_size_;
  absl::Span<const std::string> mounts_;
  std::string prog_dir_;
  absl::Mutex worker_map_mu_;
  absl::flat_hash_map<std::string, std::queue<WorkerInfo>> worker_map_
      ABSL_GUARDED_BY(worker_map_mu_);
  std::string_view callback_socket_;

  absl::Status PopulateWorkerQueue(std::string_view code_token,
                                   std::string_view prog_path, int num_workers)
      ABSL_LOCKS_EXCLUDED(worker_map_mu_);

  // Creates a new worker and returns relevant information about the worker.
  absl::StatusOr<WorkerInfo> CreateAndRunWorker(std::string_view prog_path);

  // Clears worker queue by terminating the worker and closing pipes.
  absl::Status ClearWorkerMap() ABSL_LOCKS_EXCLUDED(worker_map_mu_);

  // Starts async (detached thread) creation of replacement worker.
  // Returns WorkerInfo for a pre-created worker if one exists in the queue. If
  // not, waits for a worker to be created and returns it. exists
  absl::StatusOr<WorkerInfo> GetWorker(std::string_view code_token)
      ABSL_LOCKS_EXCLUDED(worker_map_mu_);
};
}  // namespace privacy_sandbox::server_common::gvisor

#endif  // SRC_ROMA_GVISOR_CONTAINER_SANDBOX_POOL_MANAGER_H_
