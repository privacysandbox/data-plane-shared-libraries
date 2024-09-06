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

#ifndef ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_HEAP_SNAPSHOT_PARSER_H_
#define ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_HEAP_SNAPSHOT_PARSER_H_

#include <string>

#include "include/v8.h"

namespace google::scp::roma::sandbox::js_engine::v8_js_engine {

class HeapSnapshotParser : public v8::OutputStream {
 public:
  static constexpr int kChunkSize = 1024;

  explicit HeapSnapshotParser(std::string& jsonOutput)
      : jsonOutput_(jsonOutput) {}

  ~HeapSnapshotParser() override = default;

  int GetChunkSize() override { return kChunkSize; }

  void EndOfStream() override {}

  v8::OutputStream::WriteResult WriteAsciiChunk(char* data, int size) override {
    jsonOutput_.append(data, size);
    return v8::OutputStream::WriteResult::kContinue;
  }

 private:
  std::string& jsonOutput_;  // Reference to the string to store the JSON data
};
}  // namespace google::scp::roma::sandbox::js_engine::v8_js_engine

#endif  // ROMA_SANDBOX_JS_ENGINE_V8_ENGINE_HEAP_SNAPSHOT_PARSER_H_
