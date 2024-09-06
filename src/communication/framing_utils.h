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

#ifndef COMMUNICATION_FRAMING_UTILS_H_
#define COMMUNICATION_FRAMING_UTILS_H_

#include <stddef.h>

namespace privacy_sandbox::server_common {

// Gets size of the complete payload including the preamble expected by
// client, which is: 1 byte (containing version, compression details), 4 bytes
// indicating the length of the actual encoded response and any other padding
// required to make the complete payload a power of 2.
size_t GetEncodedDataSize(size_t encapsulated_payload_size,
                          size_t min_result_bytes);
}  // namespace privacy_sandbox::server_common

#endif  // COMMUNICATION_FRAMING_UTILS_H_
