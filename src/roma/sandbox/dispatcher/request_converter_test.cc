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

#include "src/roma/sandbox/dispatcher/request_converter.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

#include "absl/container/flat_hash_map.h"
#include "src/core/test/utils/proto_test_utils.h"
#include "src/roma/interface/roma.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"
#include "src/util/protoutil.h"

namespace google::scp::roma::sandbox::dispatcher::test {
namespace {

inline constexpr char kTestId[] = "test-id";
inline constexpr char kTestVersion[] = "test-version";
inline constexpr char kTestHandlerName[] = "test-handler";
inline constexpr char kTestTag[] = "test-tag";
inline constexpr char kTestVal[] = "test-val";
inline constexpr char kTestInput[] = "test-input";
inline constexpr char kTestJs[] = "function noOp{}";

using ::google::scp::core::test::EqualsProto;
using ::worker_api::WorkerParamsProto;

TEST(RequestConverted, ConvertsInvocationRequestToProto) {
  InvocationRequest<std::string> invocation_request = {
      .id = kTestId,
      .version_string = kTestVersion,
      .handler_name = kTestHandlerName,
      .tags = {{kTestTag, kTestVal},
               {std::string(google::scp::roma::kWasmCodeArrayName), kTestVal}},
      .input = {kTestInput}};
  WorkerParamsProto worker_params_proto =
      privacy_sandbox::server_common::ParseTextOrDie<WorkerParamsProto>(R"pb(
        metadata { key: "CodeVersion" value: "test-version" }
        metadata { key: "HandlerName" value: "test-handler" }
        metadata { key: "RequestAction" value: "Execute" }
        metadata { key: "roma.request.id" value: "test-id" }
        metadata { key: "roma.request.wasm_array_name" value: "test-val" }
        metadata { key: "test-tag" value: "test-val" }
        input_strings { inputs: "test-input" }
      )pb");
  EXPECT_THAT(RequestToProto(std::move(invocation_request)),
              EqualsProto(worker_params_proto));
}

TEST(RequestConverted, ConvertsCodeObjectRequestToProto) {
  CodeObject code_object = {
      .id = kTestId,
      .version_string = kTestVersion,
      .js = kTestJs,
      .tags = {{kTestTag, kTestVal},
               {std::string(google::scp::roma::kWasmCodeArrayName), kTestVal}}};
  WorkerParamsProto worker_params_proto =
      privacy_sandbox::server_common::ParseTextOrDie<WorkerParamsProto>(R"pb(
        code: "function noOp{}"
        metadata { key: "CodeVersion" value: "test-version" }
        metadata { key: "RequestAction" value: "Load" }
        metadata { key: "RequestType" value: "JS" }
        metadata { key: "roma.request.id" value: "test-id" }
        metadata { key: "roma.request.wasm_array_name" value: "test-val" }
        metadata { key: "test-tag" value: "test-val" }
        wasm: ""
      )pb");
  EXPECT_THAT(RequestToProto(std::move(code_object)),
              EqualsProto(worker_params_proto));
}

}  // namespace
}  // namespace google::scp::roma::sandbox::dispatcher::test
