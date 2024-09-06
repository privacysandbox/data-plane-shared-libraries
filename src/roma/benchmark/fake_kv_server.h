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
#include "src/roma/roma_service/roma_service.h"

namespace google::scp::roma::benchmark {
// From:
// https://github.com/privacysandbox/fledge-key-value-service/blob/main/components/udf/code_config.h
struct CodeConfig {
  // Only one of js or wasm needs to be set.
  // If both are, js will have priority.
  std::string js;
  std::string wasm;
  std::string udf_handler_name;
};

// This class is used for benchmarking the way that the FLEDGE Key/Value Server
// uses the ROMA library.
//
// It's a loose approximation of the code here:
// https://github.com/privacysandbox/fledge-key-value-service/blob/main/components/udf/udf_client.h
//
// Key differences are:
// * This code will abort on failures, we're only benchmarking the happy path.
// * Some config (e.g. timeouts) is hardcoded.
class FakeKvServer {
 public:
  explicit FakeKvServer(Config<> config);

  ~FakeKvServer();

  // Not copyable or movable
  FakeKvServer(const FakeKvServer&) = delete;
  FakeKvServer& operator=(const FakeKvServer&) = delete;

  // Executes the UDF with the given keys. Code object must be set before making
  // this call.
  std::string ExecuteCode(const std::vector<std::string> keys);

  // Sets the JS or WASM code object that will be used for UDF execution
  void SetCodeObject(CodeConfig code_config);

 private:
  std::string handler_name_;
  std::unique_ptr<google::scp::roma::sandbox::roma_service::RomaService<>>
      roma_service_;
};

}  // namespace google::scp::roma::benchmark
