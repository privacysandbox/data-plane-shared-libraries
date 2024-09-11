// Portions Copyright (c) Microsoft Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <stdlib.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/synchronization/notification.h"
#include "src/cpio/client_providers/global_cpio/global_cpio.h"
#include "src/public/cpio/interface/cpio.h"

using google::scp::core::AsyncContext;
using google::scp::cpio::client_providers::GetSessionTokenRequest;
using google::scp::cpio::client_providers::GetSessionTokenResponse;
using google::scp::cpio::client_providers::GlobalCpio;

/*
This tool fetches JWT auth token from IDP (Managed Identity in production) and
write it to stdout.
*/

ABSL_FLAG(std::string, output_path, "fetch_auth_token_out",
          "Path to the output of this tool");

ABSL_FLAG(std::string, get_token_url, "get_token_url",
          "http://127.0.0.1:8000/metadata/identity/oauth2/"
          "token?api-version=2018-02-01");

int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);

  const auto get_token_url = absl::GetFlag(FLAGS_get_token_url);
  setenv("AZURE_BA_PARAM_GET_TOKEN_URL", get_token_url.c_str(), 0);

  // Setup
  google::scp::cpio::CpioOptions cpio_options;
  cpio_options.log_option = google::scp::cpio::LogOption::kConsoleLog;
  CHECK(google::scp::cpio::Cpio::InitCpio(cpio_options).Successful())
      << "Failed to initialize CPIO library";
  auto auth_token_provider =
      &GlobalCpio::GetGlobalCpio().GetAuthTokenProvider();

  // Fetch token
  auto request = std::make_shared<GetSessionTokenRequest>();
  absl::Notification finished;
  AsyncContext<GetSessionTokenRequest, GetSessionTokenResponse>
      get_token_context(std::move(request), [&finished](auto& context) {
        CHECK(context.result.Successful()) << "GetSessionTokenRequest failed";
        CHECK(context.response->session_token->size() > 0)
            << "Session token needs to have length more than zero";

        const std::filesystem::path output_path =
            absl::GetFlag(FLAGS_output_path);
        if (output_path.has_parent_path()) {
          create_directories(output_path.parent_path());
        }
        std::ofstream fout(output_path);
        fout << *context.response->session_token;

        finished.Notify();
      });
  CHECK(auth_token_provider->GetSessionToken(get_token_context).Successful())
      << "Failed to run auth_token_provider";
  finished.WaitForNotification();

  // Tear down
  google::scp::cpio::Cpio::ShutdownCpio(cpio_options);

  return 0;
}
