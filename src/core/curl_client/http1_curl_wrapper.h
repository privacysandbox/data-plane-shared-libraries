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

#ifndef CORE_CURL_CLIENT_HTTP1_CURL_WRAPPER_H_
#define CORE_CURL_CLIENT_HTTP1_CURL_WRAPPER_H_

#include <memory>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "absl/time/time.h"
#include "src/core/interface/http_client_interface.h"
#include "src/core/interface/http_types.h"
#include "src/core/interface/type_def.h"
#include "src/public/core/interface/execution_result.h"

namespace google::scp::core {
// Custom deleters for wrapping the raw pointers used in libcurl in unique_ptrs.
struct CurlHandleDeleter {
  void operator()(CURL* ptr) { curl_easy_cleanup(ptr); }
};

struct CurlListDeleter {
  void operator()(curl_slist* ptr) { curl_slist_free_all(ptr); }
};

// Wrapper around CURL to enable easy HTTP1 requests.
class Http1CurlWrapper {
 public:
  // Makes a Http1CurlWrapper and sets up the necessary options for CURL.
  // TODO(b/321792175): Update to return a unique_ptr
  static ExecutionResultOr<std::shared_ptr<Http1CurlWrapper>> MakeWrapper();
  explicit Http1CurlWrapper(CURL* curl);
  Http1CurlWrapper() = default;

  // Performs the request. Logs any error that occurs and returns the status of
  // the request if it failed or an HttpResponse.
  virtual ExecutionResultOr<HttpResponse> PerformRequest(
      const HttpRequest& request,
      const absl::Duration& timeout = google::scp::core::kHttpRequestTimeout);

  virtual ~Http1CurlWrapper() = default;

 private:
  // Adds headers to the CURL instance. Returns the curl_slist containing the
  // headers.
  ExecutionResultOr<std::unique_ptr<curl_slist, CurlListDeleter>>
  AddHeadersToRequest(const std::shared_ptr<HttpHeaders>& headers);

  // Sets up the mechanism for acquiring the headers returned in the Http
  // response into returned_header_destination.
  void SetUpResponseHeaderHandler(HttpHeaders* returned_header_destination);

  // Sets up the mechanism for uploading the body of a POST request.
  void SetUpPostData(const std::shared_ptr<std::string>& body);

  // Sets up the mechanism for uploading the body of a PUT request.
  void SetUpPutData(const std::shared_ptr<std::string>& body);

  std::unique_ptr<CURL, CurlHandleDeleter> curl_;
};

// Simple class to provide Http1CurlWrappers in clients.
class Http1CurlWrapperProvider {
 public:
  virtual ExecutionResultOr<std::shared_ptr<Http1CurlWrapper>> MakeWrapper();

  Http1CurlWrapperProvider() = default;
  virtual ~Http1CurlWrapperProvider() = default;
};

}  // namespace google::scp::core

#endif  // CORE_CURL_CLIENT_HTTP1_CURL_WRAPPER_H_
