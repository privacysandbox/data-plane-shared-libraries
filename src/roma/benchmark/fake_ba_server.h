/*
 * Copyright 2023 Google LLC
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

#include <memory>
#include <string>
#include <vector>

#include "src/roma/config/config.h"
#include "src/roma/interface/roma.h"
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::benchmark {
using DispatchConfig = google::scp::roma::Config<>;
using DispatchRequest = google::scp::roma::InvocationSharedRequest<>;

// This class is used for benchmarking the way that the FLEDGE Bidding and
// Auction Services use the ROMA library.
//
// It's a loose approximation of the code here:
// https://github.com/privacysandbox/bidding-auction-servers/blob/main/services/common/clients/code_dispatcher/v8_dispatcher.h
//
// Key differences are:
// * This code will abort on failures, we're only benchmarking the happy path.
// * Some config (e.g. timeouts) is hardcoded.
class FakeBaServer {
 public:
  explicit FakeBaServer(DispatchConfig config);

  ~FakeBaServer();

  // Not copyable or movable
  FakeBaServer(const FakeBaServer&) = delete;
  FakeBaServer& operator=(const FakeBaServer&) = delete;

  // Synchronously loads code.
  void LoadSync(std::string_view version, std::string_view js) const;

  // Unlike the B&A codebase, this call blocks until all of the execution is
  // finished.
  void BatchExecute(std::vector<DispatchRequest>& batch) const;

 private:
  std::unique_ptr<google::scp::roma::sandbox::roma_service::RomaService<>>
      roma_service_;
};

}  // namespace google::scp::roma::benchmark
