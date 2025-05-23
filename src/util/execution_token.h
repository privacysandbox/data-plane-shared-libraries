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

#ifndef ROMA_INTERFACE_EXECUTION_TOKEN_H_
#define ROMA_INTERFACE_EXECUTION_TOKEN_H_

#include <string>

namespace google::scp::roma {
struct ExecutionToken {
  std::string value;
};
}  // namespace google::scp::roma

#endif  // ROMA_INTERFACE_EXECUTION_TOKEN_H_
