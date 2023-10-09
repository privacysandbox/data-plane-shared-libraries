// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include "core/interface/errors.h"
#include "public/core/interface/execution_result.h"
#include "public/cpio/interface/error_codes.h"

namespace google::scp::core::errors {
REGISTER_COMPONENT_CODE(SC_AWS_INSTANCE_SERVICE_FACTORY, 0x022A)

DEFINE_ERROR_CODE(SC_AWS_INSTANCE_SERVICE_FACTORY_HTTP2_CLIENT_NOT_FOUND,
                  SC_AWS_INSTANCE_SERVICE_FACTORY, 0x0001,
                  "Cannot find Http2Client",
                  HttpStatusCode::INTERNAL_SERVER_ERROR)
}  // namespace google::scp::core::errors
