/*
 * Copyright 2022 Google LLC
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

#include "global_cpio.h"

#include <memory>
#include <utility>

#include "absl/log/check.h"
#include "src/cpio/client_providers/interface/cpio_provider_interface.h"

namespace google::scp::cpio::client_providers {
static std::unique_ptr<CpioProviderInterface> cpio_instance_;

CpioProviderInterface& GlobalCpio::GetGlobalCpio() {
  CHECK(cpio_instance_.get() != nullptr)
      << "Cpio must be initialized with Cpio::InitCpio before client "
         "use";
  return *cpio_instance_;
}

void GlobalCpio::SetGlobalCpio(std::unique_ptr<CpioProviderInterface> cpio) {
  cpio_instance_ = std::move(cpio);
}

void GlobalCpio::ShutdownGlobalCpio() { cpio_instance_ = nullptr; }
}  // namespace google::scp::cpio::client_providers
