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

#include "arraybuffer.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <filesystem>
#include <memory>
#include <numeric>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "src/roma/benchmark/flatbuffers/arraybuffer_flatbuffer_js.h"
#include "src/util/process_util.h"

#include "arraybuffer_js.h"

namespace arraybuffer =
    google::scp::roma::sandbox::js_engine::v8_js_engine::arraybuffer;
using arraybuffer::CompileRun;
using arraybuffer::Environment;
using arraybuffer::ExpectInt64;
using arraybuffer::ExpectString;
using arraybuffer::LocalContext;
using arraybuffer::MapFlatbufferFile;
using arraybuffer::v8_str;

class V8ArrayBufferTest : public ::testing::Test {
 protected:
  void SetUp() override {
    create_params_.array_buffer_allocator =
        v8::ArrayBuffer::Allocator::NewDefaultAllocator();
    isolate_ = v8::Isolate::New(create_params_);
  }

  void TearDown() override {
    isolate_->Dispose();
    delete create_params_.array_buffer_allocator;
  }

  v8::Isolate::CreateParams create_params_;
  v8::Isolate* isolate_;
};

struct MmapParams {
  std::string filename;
  int64_t num_elements;
  std::string num_elements_abbrev;
  std::string data_layout;
  std::string test_name;
  std::vector<std::pair<std::string_view, std::string_view>> expected_keyvalues;
};

class V8ArrayBufferTestP : public V8ArrayBufferTest,
                           public ::testing::WithParamInterface<MmapParams> {};

TEST_F(V8ArrayBufferTest, TinyArrayBuffer) {
  LocalContext env(isolate_);
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);
  constexpr size_t bufsize = 8;
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, bufsize);
  ASSERT_NE(array_buffer->GetBackingStore(), nullptr);
  ASSERT_FALSE(array_buffer->GetBackingStore()->IsShared());
  ASSERT_FALSE(v8::ArrayBuffer::NewBackingStore(isolate, 8)->IsShared());
  ASSERT_EQ(bufsize, array_buffer->ByteLength());
}

TEST_F(V8ArrayBufferTest, SmallArrayBuffer) {
  LocalContext env(isolate_);
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  constexpr int64_t bufsize = 1024;
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate, bufsize);
  ASSERT_NE(array_buffer->GetBackingStore(), nullptr);
  ASSERT_EQ(array_buffer->GetBackingStore()->ByteLength(), bufsize);
}

TEST_F(V8ArrayBufferTest, FixedArrayBuffer) {
  const std::array<uint8_t, 10> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
  const size_t data_len = data.size();
  const uintptr_t address = reinterpret_cast<uintptr_t>(data.data());
  void* store_ptr = reinterpret_cast<void*>(address);

  LocalContext env(isolate_);
  v8::Isolate* isolate = env->GetIsolate();
  v8::HandleScope handle_scope(isolate);

  const auto deleter_fn = [](void*, size_t, void*) {};
  std::shared_ptr<v8::BackingStore> bstore = v8::ArrayBuffer::NewBackingStore(
      store_ptr, data_len, deleter_fn, nullptr);
  v8::Local<v8::ArrayBuffer> array_buffer =
      v8::ArrayBuffer::New(isolate_, std::move(bstore));
  ASSERT_NE(array_buffer->GetBackingStore(), nullptr);
  ASSERT_EQ(data_len, array_buffer->ByteLength());
}

void MMapDeleter(void* data, size_t length, void* deleter_data) {
  PCHECK(::munmap(deleter_data, length) != -1);
}

TEST_P(V8ArrayBufferTestP, MMapArrayBuffer) {
  LocalContext env(isolate_);
  v8::Isolate* isolate = env->GetIsolate();

  // NOTE: calling isolate->Enter and Exit is critical to the success of the
  // use of the external backing store
  isolate->Enter();

  v8::HandleScope handle_scope(isolate);

  const std::filesystem::path mmap_base_path("src/roma/benchmark/flatbuffers");
  const MmapParams& param = GetParam();
  v8::Local<v8::Context> ctx = env.local();

  for (const auto& c : {kModuleJs, kTextDecoderJs, kFlatbufferJs, kUtilJs}) {
    (void)CompileRun(ctx, c);
  }

  size_t data_len;
  MapFlatbufferFile(env, mmap_base_path / param.filename, param.data_layout,
                    "kv_fb_arrbuf", "kvdata", data_len);

  ExpectInt64(ctx, "kv_fb_arrbuf.byteLength;", data_len);
  ExpectInt64(ctx, "kvdata.Length();", param.num_elements);
  for (const auto& [k, v] : param.expected_keyvalues) {
    ExpectString(ctx, k, v);
  }
  isolate->Exit();
}

std::vector<MmapParams> GetParamsWithFilenames() {
  const MmapParams kMMapTestArgs[] = {
      {
          .num_elements = 1'000,
          .num_elements_abbrev = "1K",
          .expected_keyvalues =  // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  "kvdata.KeyAt(12);",
                  "k-00000000000000010000000012",
              },
              {
                  "kvdata.ValueAt(2);",
                  "v--00000000000000010000000002..............................."
                  "............................................................"
                  ".........--v",
              },
              {
                  "kvdata.OffsetLookup(10).val;",
                  "v--00000000000000010000000010..............................."
                  "............................................................"
                  ".........--v",
              },
          },
      },
      {
          .num_elements = 10'000,
          .num_elements_abbrev = "10K",
          .expected_keyvalues =  // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  "kvdata.OffsetLookup(8_245).key;",
                  "k-00000000000000010000008245",
              },
              {
                  "KVStr(kvdata.OffsetLookup(2));",
                  "{key: k-00000000000000010000000002, val: "
                  "v--00000000000000010000000002..............................."
                  ".."
                  "............................................................"
                  ".."
                  ".....--v}",
              },
              {
                  "kvdata.ValueAt(5_102);",
                  "v--00000000000000010000005102..............................."
                  ".."
                  "............................................................"
                  ".."
                  ".....--v",
              },
          },
      },
      {
          .num_elements = 100'000,
          .num_elements_abbrev = "100K",
          .expected_keyvalues =  // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  "kvdata.KeyAt(92_245);",
                  "k-00000000000000010000092245",
              },
              {
                  "kvdata.ValueAt(54_102);",
                  "v--00000000000000010000054102..............................."
                  ".."
                  "............................................................"
                  ".."
                  ".....--v",
              },
          },
      },
      {
          .num_elements = 1'000'000,
          .num_elements_abbrev = "1M",
          .expected_keyvalues =  // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  "kvdata.KeyAt(950_120);",
                  "k-00000000000000010000950120",
              },
              {
                  "kvdata.ValueAt(500_010);",
                  "v--00000000000000010000500010..............................."
                  ".."
                  "............................................................"
                  ".."
                  ".....--v",
              },
          },
      },
      {
          .num_elements = 5'000'000,
          .num_elements_abbrev = "5M",
          .expected_keyvalues =  // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  "kvdata.KeyAt(4_950_120);",
                  "k-00000000000000010004950120",
              },
              {
                  "kvdata.ValueAt(2_500_010);",
                  "v--00000000000000010002500010..............................."
                  ".."
                  "............................................................"
                  ".."
                  ".....--v",
              },
          },
      },
      {
          .num_elements = 10'000'000,
          .num_elements_abbrev = "10M",
          .expected_keyvalues =  // NOLINTNEXTLINE(whitespace/braces)
          {
              {
                  "kvdata.KeyAt(9_950_120);",
                  "k-00000000000000010009950120",
              },
              {
                  "kvdata.ValueAt(2_500_010);",
                  "v--00000000000000010002500010..............................."
                  ".."
                  "............................................................"
                  ".."
                  ".....--v",
              },
          },
      },
  };
  constexpr std::string_view kDataLayouts[] = {
      "KVStrData",
      "KVKeyData",
      "KVDataParallel",
  };

  std::vector<MmapParams> params;
  for (auto p : kMMapTestArgs) {
    for (const auto& data_layout : kDataLayouts) {
      p.filename = absl::StrCat(absl::AsciiStrToLower(data_layout), "_fbs_",
                                p.num_elements_abbrev, ".fbs");
      p.data_layout = data_layout;
      p.test_name = absl::StrCat("V8_Mmap_Flatbuffers_", p.data_layout, "_",
                                 p.num_elements_abbrev);
      params.push_back(p);
    }
  }
  return params;
}

INSTANTIATE_TEST_SUITE_P(
    V8ArrayBufferMMap, V8ArrayBufferTestP,
    ::testing::ValuesIn(GetParamsWithFilenames()),
    [](const testing::TestParamInfo<V8ArrayBufferTestP::ParamType>& info)
        -> std::string { return info.param.test_name; });

testing::Environment* const test_env =
    testing::AddGlobalTestEnvironment(new Environment());
