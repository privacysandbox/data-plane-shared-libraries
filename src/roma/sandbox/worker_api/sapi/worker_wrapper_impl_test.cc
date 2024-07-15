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

#include "src/roma/sandbox/worker_api/sapi/worker_wrapper_impl.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <utility>

#include "sandboxed_api/lenval_core.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "src/roma/sandbox/constants/constants.h"
#include "src/roma/sandbox/worker_api/sapi/error_codes.h"
#include "src/roma/sandbox/worker_api/sapi/worker_init_params.pb.h"
#include "src/roma/sandbox/worker_api/sapi/worker_params.pb.h"

using google::scp::roma::sandbox::constants::kCodeVersion;
using google::scp::roma::sandbox::constants::kHandlerName;
using google::scp::roma::sandbox::constants::kRequestAction;
using google::scp::roma::sandbox::constants::kRequestActionExecute;
using google::scp::roma::sandbox::constants::kRequestType;
using google::scp::roma::sandbox::constants::kRequestTypeJavascript;
using ::testing::StrEq;

namespace google::scp::roma::sandbox::worker_api::test {
constexpr size_t kBufferSize = 1 * 1024 * 1024 /* 1Mib */;
std::unique_ptr<sandbox2::Buffer> buffer_ptr_;

static ::worker_api::WorkerInitParamsProto GetDefaultInitParams() {
  // create a sandbox2 buffer
  auto buffer = sandbox2::Buffer::CreateWithSize(kBufferSize);
  EXPECT_TRUE(buffer.ok());
  buffer_ptr_ = std::move(buffer).value();

  ::worker_api::WorkerInitParamsProto init_params;
  init_params.set_require_code_preload_for_execution(false);
  init_params.set_native_js_function_comms_fd(-1);
  init_params.set_server_address("");
  init_params.mutable_native_js_function_names()->Clear();
  init_params.mutable_rpc_method_names()->Clear();
  init_params.set_js_engine_initial_heap_size_mb(0);
  init_params.set_js_engine_maximum_heap_size_mb(0);
  init_params.set_js_engine_max_wasm_memory_number_of_pages(0);
  init_params.set_request_and_response_data_buffer_fd(buffer_ptr_->fd());
  init_params.set_request_and_response_data_buffer_size_bytes(kBufferSize);
  init_params.set_skip_v8_cleanup(true);
  init_params.mutable_v8_flags()->Clear();
  init_params.set_enable_profilers(false);
  return init_params;
}

TEST(WorkerWrapperImplTest,
     CanRunCodeThroughWrapperWithoutPreloadSharedWithBuffer) {
  auto init_params = GetDefaultInitParams();
  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  EXPECT_EQ(SapiStatusCode::kOk, ::InitFromSerializedData(&sapi_init_params));

  EXPECT_EQ(SapiStatusCode::kOk, ::Run());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int serialized_size = params_proto.ByteSizeLong();
  ASSERT_TRUE(
      params_proto.SerializeToArray(buffer_ptr_->data(), serialized_size));

  size_t output_serialized_size_ptr;
  sapi::LenValStruct sapi_worker_params;
  auto result = ::RunCodeFromSerializedData(
      &sapi_worker_params, serialized_size, &output_serialized_size_ptr);

  ASSERT_EQ(SapiStatusCode::kOk, result);

  ::worker_api::WorkerParamsProto response_proto;
  ASSERT_TRUE(response_proto.ParseFromArray(buffer_ptr_->data(),
                                            output_serialized_size_ptr));
  EXPECT_THAT(response_proto.response(), StrEq(R"js("Hi there from JS :)")js"));

  EXPECT_EQ(SapiStatusCode::kOk, ::Stop());
}

// Function object which invokes 'free' on its parameter, which must be
// a pointer. Can be used to store malloc-allocated pointers in std::unique_ptr:
//
// std::unique_ptr<int, FreeDeleter> foo_ptr(
//     static_cast<int*>(malloc(sizeof(int))));
struct FreeDeleter {
  inline void operator()(void* ptr) const { free(ptr); }
};

sapi::LenValStruct CreateLenValStruct(std::string_view in_str) {
  // We need to copy the serialized proto because RunCodeFromSerializedData()
  // will take ownership of the data that it contains and we don't want to free
  // the same memory twice.
  std::unique_ptr<uint8_t[], FreeDeleter> out_str(
      static_cast<uint8_t*>(malloc(in_str.size())));
  memcpy(out_str.get(), in_str.data(), in_str.size());
  return sapi::LenValStruct{in_str.size(), out_str.release()};
}

TEST(WorkerWrapperImplTest,
     CanRunCodeThroughWrapperWithoutPreloadSharedWithLenValStruct) {
  auto init_params = GetDefaultInitParams();
  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  EXPECT_EQ(SapiStatusCode::kOk, ::InitFromSerializedData(&sapi_init_params));

  EXPECT_EQ(SapiStatusCode::kOk, ::Run());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::string serialized_worker_params;
  ASSERT_TRUE(params_proto.SerializeToString(&serialized_worker_params));
  auto sapi_worker_params = CreateLenValStruct(serialized_worker_params);

  size_t output_serialized_size_ptr;
  auto result = ::RunCodeFromSerializedData(&sapi_worker_params, 0,
                                            &output_serialized_size_ptr);
  ASSERT_EQ(SapiStatusCode::kOk, result);

  // Take ownership of the response bytes, these will have been malloc'd by
  // RunCodeFromSerializedData() if it was successful.
  std::unique_ptr<uint8_t[], FreeDeleter> response_bytes(
      static_cast<uint8_t*>(sapi_worker_params.data));

  ::worker_api::WorkerParamsProto response_proto;
  ASSERT_TRUE(response_proto.ParseFromArray(buffer_ptr_->data(),
                                            output_serialized_size_ptr));
  EXPECT_THAT(response_proto.response(), StrEq(R"js("Hi there from JS :)")js"));

  EXPECT_EQ(SapiStatusCode::kOk, ::Stop());
}

TEST(WorkerWrapperImplTest, OverSizeResponseSharedWithLenValStruct) {
  auto init_params = GetDefaultInitParams();
  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  EXPECT_EQ(SapiStatusCode::kOk, ::InitFromSerializedData(&sapi_init_params));

  EXPECT_EQ(SapiStatusCode::kOk, ::Run());

  ::worker_api::WorkerParamsProto params_proto;

  // generate oversize response (2 * 1024 * 1024 Bytes==2MB)
  params_proto.set_code(R"JS_CODE(
    function cool_func() {
      const dummy_string = 'x'.repeat(2 * 1024 * 1024);
      return dummy_string;
    }
  )JS_CODE");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  std::string serialized_worker_params;
  ASSERT_TRUE(params_proto.SerializeToString(&serialized_worker_params));
  auto sapi_worker_params = CreateLenValStruct(serialized_worker_params);

  size_t output_serialized_size_ptr;
  auto result = ::RunCodeFromSerializedData(&sapi_worker_params, 0,
                                            &output_serialized_size_ptr);
  ASSERT_EQ(SapiStatusCode::kOk, result);

  // Take ownership of the response bytes, these will have been malloc'd by
  // RunCodeFromSerializedData() if it was successful.
  std::unique_ptr<unsigned char[], FreeDeleter> response_bytes(
      static_cast<unsigned char*>(sapi_worker_params.data));

  ::worker_api::WorkerParamsProto response_proto;
  ASSERT_TRUE(response_proto.ParseFromArray(sapi_worker_params.data,
                                            sapi_worker_params.size));
  EXPECT_EQ(SapiStatusCode::kOk, ::Stop());
}

TEST(WorkerWrapperImplTest, CanRunCodeWithBufferShareOnly) {
  auto init_params = GetDefaultInitParams();
  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  EXPECT_EQ(SapiStatusCode::kOk, ::InitFromSerializedData(&sapi_init_params));

  EXPECT_EQ(SapiStatusCode::kOk, ::Run());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int serialized_size = params_proto.ByteSizeLong();
  ASSERT_TRUE(
      params_proto.SerializeToArray(buffer_ptr_->data(), serialized_size));

  size_t output_serialized_size_ptr;
  auto result =
      ::RunCodeFromBuffer(serialized_size, &output_serialized_size_ptr);

  ASSERT_EQ(SapiStatusCode::kOk, result);

  ::worker_api::WorkerParamsProto response_proto;
  ASSERT_TRUE(response_proto.ParseFromArray(buffer_ptr_->data(),
                                            output_serialized_size_ptr));
  EXPECT_THAT(response_proto.response(), StrEq(R"js("Hi there from JS :)")js"));

  EXPECT_EQ(SapiStatusCode::kOk, ::Stop());
}

TEST(WorkerWrapperImplTest,
     ShouldFailRunCodeWithBufferShareOnlyIfResponseOversize) {
  auto init_params = GetDefaultInitParams();
  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  EXPECT_EQ(SapiStatusCode::kOk, ::InitFromSerializedData(&sapi_init_params));

  EXPECT_EQ(SapiStatusCode::kOk, ::Run());

  ::worker_api::WorkerParamsProto params_proto;
  // generate oversize response (2 * 1024 * 1024 Bytes==2MB)
  params_proto.set_code(R"JS_CODE(
    function cool_func() {
      const dummy_string = 'x'.repeat(2 * 1024 * 1024);
      return dummy_string;
    }
  )JS_CODE");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int serialized_size = params_proto.ByteSizeLong();
  ASSERT_TRUE(
      params_proto.SerializeToArray(buffer_ptr_->data(), serialized_size));

  size_t output_serialized_size_ptr;
  auto result =
      ::RunCodeFromBuffer(serialized_size, &output_serialized_size_ptr);
  EXPECT_EQ(result, SapiStatusCode::kResponseLargerThanBuffer);

  EXPECT_EQ(SapiStatusCode::kOk, ::Stop());
}

TEST(WorkerWrapperImplTest,
     FailsToRunCodeWhenPreloadIsRequiredAndExecuteIsSent) {
  auto init_params = GetDefaultInitParams();
  init_params.set_require_code_preload_for_execution(true);
  init_params.set_require_code_preload_for_execution(true);

  std::string serialized_init_params;
  ASSERT_TRUE(init_params.SerializeToString(&serialized_init_params));

  sapi::LenValStruct sapi_init_params(
      serialized_init_params.size(),
      static_cast<void*>(serialized_init_params.data()));

  EXPECT_EQ(SapiStatusCode::kOk, ::InitFromSerializedData(&sapi_init_params));

  EXPECT_EQ(SapiStatusCode::kOk, ::Run());

  ::worker_api::WorkerParamsProto params_proto;
  params_proto.set_code(
      R"js(function cool_func() { return "Hi there from JS :)" })js");
  (*params_proto.mutable_metadata())[kRequestType] = kRequestTypeJavascript;
  (*params_proto.mutable_metadata())[kHandlerName] = "cool_func";
  (*params_proto.mutable_metadata())[kCodeVersion] = "1";
  (*params_proto.mutable_metadata())[kRequestAction] = kRequestActionExecute;

  int serialized_size = params_proto.ByteSizeLong();
  EXPECT_TRUE(
      params_proto.SerializeToArray(buffer_ptr_->data(), serialized_size));

  size_t output_serialized_size_ptr;
  sapi::LenValStruct sapi_worker_params;
  auto result = ::RunCodeFromSerializedData(
      &sapi_worker_params, serialized_size, &output_serialized_size_ptr);
  EXPECT_NE(result, SapiStatusCode::kOk);

  EXPECT_EQ(SapiStatusCode::kOk, ::Stop());
}
}  // namespace google::scp::roma::sandbox::worker_api::test
