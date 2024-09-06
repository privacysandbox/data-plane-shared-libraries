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

#include <memory>

#include "include/libplatform/libplatform.h"
#include "include/v8.h"

int main() {
  v8::V8::SetFlagsFromString("--help");
  std::unique_ptr<v8::Platform> v8_platform =
      v8::platform::NewDefaultPlatform();
  v8::V8::InitializePlatform(v8_platform.get());
  v8::V8::Initialize();
  v8::V8::Dispose();
  v8::V8::DisposePlatform();
  return 0;
}
