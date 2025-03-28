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

#ifndef ROMA_SANDBOX_CONSTANTS_CONSTANTS_H_
#define ROMA_SANDBOX_CONSTANTS_CONSTANTS_H_

#include <string_view>

namespace google::scp::roma::sandbox::constants {
inline constexpr std::string_view kRequestType = "RequestType";
inline constexpr std::string_view kRequestTypeJavascript = "JS";
inline constexpr std::string_view kRequestTypeWasm = "WASM";
inline constexpr std::string_view kRequestTypeJavascriptWithWasm = "JSAndWASM";

inline constexpr std::string_view kHandlerName = "HandlerName";

inline constexpr std::string_view kInputType = "InputType";
inline constexpr std::string_view kInputTypeBytes = "InputTypeBytes";

inline constexpr std::string_view kMinLogLevel = "MinLogLevel";
inline constexpr std::string_view kRequestUuid = "RequestUuid";
inline constexpr std::string_view kCodeVersion = "CodeVersion";
inline constexpr std::string_view kRequestAction = "RequestAction";
inline constexpr std::string_view kRequestActionLoad = "Load";
inline constexpr std::string_view kRequestActionExecute = "Execute";
inline constexpr std::string_view kJsEngineOneTimeSetupWasmPagesKey =
    "MaxWasmNumberOfPages";
inline constexpr std::string_view kJsEngineOneTimeSetupV8FlagsKey = "V8Flags";

inline constexpr std::string_view kRequestId = "roma.request.id";

inline constexpr int kCodeVersionCacheSize = 5;

inline constexpr std::string_view kWasmMemPagesV8PlatformFlag =
    "--wasm_max_mem_pages=";
inline constexpr size_t kMaxNumberOfWasm32BitMemPages = 65536;

// Invalid file descriptor value.
inline constexpr int kBadFd = -1;
}  // namespace google::scp::roma::sandbox::constants

#endif  // ROMA_SANDBOX_CONSTANTS_CONSTANTS_H_
