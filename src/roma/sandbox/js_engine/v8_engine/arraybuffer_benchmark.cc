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

#include <gtest/gtest.h>

#include <fcntl.h>
#include <sys/mman.h>

#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include <benchmark/benchmark.h>

#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/notification.h"
#include "include/libplatform/libplatform.h"
#include "include/v8-context.h"
#include "include/v8-initialization.h"
#include "include/v8-isolate.h"
#include "include/v8-local-handle.h"
#include "include/v8-primitive.h"
#include "include/v8-script.h"
#include "src/roma/benchmark/flatbuffers/arraybuffer_flatbuffer_js.h"
#include "src/util/process_util.h"

#include "arraybuffer.h"

namespace arraybuffer =
    google::scp::roma::sandbox::js_engine::v8_js_engine::arraybuffer;
using arraybuffer::CompileRun;
using arraybuffer::Environment;
using arraybuffer::ExpectInt64;
using arraybuffer::ExpectString;
using arraybuffer::GetInt64;
using arraybuffer::LocalContext;
using arraybuffer::MapFlatbufferFile;
using arraybuffer::v8_str;

namespace {

struct MmapParams {
  std::string filename;
  std::string algo;
  int64_t num_elements;
  std::string num_elements_abbrev;
  std::string_view code;
  std::string_view expected;
};

void BM_MmapV8ArrayBuffer(benchmark::State& state, const MmapParams& param,
                          std::string_view data_layout,
                          v8::Isolate** isolate_handle) {
  LocalContext env(*isolate_handle);
  v8::Isolate* isolate = *isolate_handle;

  // NOTE: calling isolate->Enter and Exit is critical to the success of the
  // use of the external backing store
  isolate->Enter();

  v8::HandleScope handle_scope(isolate);

  v8::Local<v8::Context> ctx = env.local();
  for (const auto& c : {kModuleJs, kTextDecoderJs, kFlatbufferJs, kUtilJs}) {
    (void)CompileRun(ctx, c);
  }

  const std::filesystem::path mmap_base_path("src/roma/benchmark/flatbuffers");
  size_t data_len;
  MapFlatbufferFile(env, mmap_base_path / param.filename, data_layout,
                    "fb_arrbuf", "kvdata", data_len);
  ExpectInt64(ctx, "fb_arrbuf.byteLength", data_len);
  ExpectInt64(ctx, "kvdata.Length();", param.num_elements);

  // determine lookup count outside timing loop
  ExpectString(ctx, param.code, param.expected);
  const int64_t num_lookups = GetInt64(ctx, "obj.num_lookups");

  for (auto _ : state) {
    ExpectString(ctx, param.code, param.expected);
  }
  state.counters["lookup_rate"] = benchmark::Counter(
      num_lookups, benchmark::Counter::kIsIterationInvariantRate);
  state.counters["num_lookups_per_iter"] = num_lookups;

  isolate->Exit();
}

const MmapParams kMMapArrayParams[] = {
    {
        .algo = "Offset",
        .num_elements = 1'000,
        .num_elements_abbrev = "1K",
        .code = "obj = kvdata.OffsetLookup(10); obj; obj.val;",
        .expected = "v--00000000000000010000000010............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "Offset",
        .num_elements = 10'000,
        .num_elements_abbrev = "10K",
        .code = "obj = kvdata.OffsetLookup(8_012); obj.val;",
        .expected = "v--00000000000000010000008012............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "Offset",
        .num_elements = 100'000,
        .num_elements_abbrev = "100K",
        .code = "obj = kvdata.OffsetLookup(18_012); obj.val;",
        .expected = "v--00000000000000010000018012............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "Offset",
        .num_elements = 1'000'000,
        .num_elements_abbrev = "1M",
        .code = "obj = kvdata.OffsetLookup(999_999); obj.val;",
        .expected = "v--00000000000000010000999999............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "Offset",
        .num_elements = 5'000'000,
        .num_elements_abbrev = "5M",
        .code = "obj = kvdata.OffsetLookup(2_474_332); obj.val;",
        .expected = "v--00000000000000010002474332............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "Offset",
        .num_elements = 10'000'000,
        .num_elements_abbrev = "10M",
        .code = "obj = kvdata.OffsetLookup(7_474_332); obj.val;",
        .expected = "v--00000000000000010007474332............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid",
        .num_elements = 1'000,
        .num_elements_abbrev = "1K",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000000499'); "
                "obj.val;",
        .expected = "v--00000000000000010000000499............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid_1",
        .num_elements = 1'000,
        .num_elements_abbrev = "1K",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000000498'); "
                "obj.val;",
        .expected = "v--00000000000000010000000498............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid",
        .num_elements = 10'000,
        .num_elements_abbrev = "10K",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000004999'); "
                "obj.val;",
        .expected = "v--00000000000000010000004999............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid_1",
        .num_elements = 10'000,
        .num_elements_abbrev = "10K",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000004998'); "
                "obj.val;",
        .expected = "v--00000000000000010000004998............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid",
        .num_elements = 100'000,
        .num_elements_abbrev = "100K",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000049999'); "
                "obj.val;",
        .expected = "v--00000000000000010000049999............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid_1",
        .num_elements = 100'000,
        .num_elements_abbrev = "100K",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000049998'); "
                "obj.val;",
        .expected = "v--00000000000000010000049998............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid",
        .num_elements = 1'000'000,
        .num_elements_abbrev = "1M",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000499999'); "
                "obj.val;",
        .expected = "v--00000000000000010000499999............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid_1",
        .num_elements = 1'000'000,
        .num_elements_abbrev = "1M",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010000499998'); "
                "obj.val;",
        .expected = "v--00000000000000010000499998............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid",
        .num_elements = 5'000'000,
        .num_elements_abbrev = "5M",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010002499999'); "
                "obj.val;",
        .expected = "v--00000000000000010002499999............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid_1",
        .num_elements = 5'000'000,
        .num_elements_abbrev = "5M",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010002499998'); "
                "obj.val;",
        .expected = "v--00000000000000010002499998............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid",
        .num_elements = 10'000'000,
        .num_elements_abbrev = "10M",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010004999999'); "
                "obj.val;",
        .expected = "v--00000000000000010004999999............................."
                    ".........................................................."
                    ".............--v",
    },
    {
        .algo = "BinSearch_Mid_1",
        .num_elements = 10'000'000,
        .num_elements_abbrev = "10M",
        .code = "obj = kvdata.BinarySearch('k-00000000000000010004999998'); "
                "obj.val;",
        .expected = "v--00000000000000010004999998............................."
                    ".........................................................."
                    ".............--v",
    },
};

v8::Isolate* isolate_ = nullptr;
v8::Isolate::CreateParams create_params_;

void IsolateSetup(const ::benchmark::State& state) {
  create_params_.array_buffer_allocator =
      v8::ArrayBuffer::Allocator::NewDefaultAllocator();
  isolate_ = v8::Isolate::New(create_params_);
}

void IsolateTeardown(const ::benchmark::State& state) {
  if (isolate_ != nullptr) {
    isolate_->Dispose();
  }
  delete create_params_.array_buffer_allocator;
}

}  // namespace

int main(int argc, char* argv[]) {
  // Setup
  std::unique_ptr<Environment> env = std::make_unique<Environment>();
  env->SetUp();

  constexpr std::string_view data_layouts[] = {
      "KVStrData",
      "KVKeyData",
      "KVDataParallel",
  };
  for (const auto& data_layout : data_layouts) {
    for (auto p : kMMapArrayParams) {
      p.filename = absl::StrCat(absl::AsciiStrToLower(data_layout), "_fbs_",
                                p.num_elements_abbrev, ".fbs");
      const std::string microbenchmark = absl::StrCat(
          "Flatbuffers_", data_layout, "_", p.num_elements_abbrev, "_", p.algo);
      benchmark::RegisterBenchmark(microbenchmark, BM_MmapV8ArrayBuffer, p,
                                   data_layout, &isolate_)
          ->Setup(IsolateSetup)
          ->Teardown(IsolateTeardown);
    }
  }

  benchmark::Initialize(&argc, argv);

  benchmark::RunSpecifiedBenchmarks();

  env.reset();
  benchmark::Shutdown();
}
