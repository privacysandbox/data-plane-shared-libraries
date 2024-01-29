#include "src/cpp/accesstoken/src/accesstoken_fetcher_manager.h"

#include <memory>

#include "public/core/interface/execution_result.h"

namespace privacy_sandbox::server_common {
using google::scp::core::AsyncContext;
using google::scp::core::Http1CurlClient;
using google::scp::core::HttpRequest;
using google::scp::core::HttpResponse;

AccessTokenClientFactory::AccessTokenClientFactory(
    const AccessTokenClientOptions& options,
    std::shared_ptr<google::scp::core::HttpClientInterface>
        http_client) noexcept
    : tokenOptions_(options), http_client_(std::move(http_client)) {}

std::unique_ptr<AccessTokenClientFactory> AccessTokenClientFactory::Create(
    const AccessTokenClientOptions& options,
    std::shared_ptr<google::scp::core::HttpClientInterface>
        http_client) noexcept {
  http_client->Init();
  http_client->Run();
  return std::make_unique<AccessTokenClientFactory>(options,
                                                    std::move(http_client));
}

/// @brief Make a REST API request
/// @param url API url
/// @param method Http method (default GET)
/// @param headers Request headers (optional)
/// @return
std::tuple<google::scp::core::ExecutionResult, std::string, int>
AccessTokenClientFactory::MakeRequest(
    const std::string& url, google::scp::core::HttpMethod method,
    const absl::btree_multimap<std::string, std::string>& headers,
    std::string request_body) {
  auto request = std::make_shared<google::scp::core::HttpRequest>();
  request->method = method;
  request->path = std::make_shared<std::string>(url);
  if (!headers.empty()) {
    request->headers =
        std::make_shared<google::scp::core::HttpHeaders>(headers);
  }
  if (!request_body.empty()) {
    request->body = google::scp::core::BytesBuffer(request_body);
  }
  google::scp::core::ExecutionResult context_result;
  absl::Notification finished;
  std::string response_body = "";
  int status_code = 404;
  AsyncContext<google::scp::core::HttpRequest, google::scp::core::HttpResponse>
      context(std::move(request),
              [&](AsyncContext<google::scp::core::HttpRequest,
                               google::scp::core::HttpResponse>& context) {
                context_result = context.result;
                if (context.response) {
                  status_code = static_cast<int>(context.response->code);
                  if (status_code < 300) {
                    const auto& bytes = *context.response->body.bytes;
                    response_body = std::string(bytes.begin(), bytes.end());
                  }
                }
                finished.Notify();
              });

  auto result = http_client_->PerformRequest(context, kRequestTimeout);

  finished.WaitForNotification();

  return {context_result, response_body, status_code};
}

std::tuple<google::scp::core::ExecutionResult, std::string, int> 
AccessTokenClientFactory::GetAccessToken() {
  // Use tokenOptions_ to retrieve the access token
    // Set http header
    absl::btree_multimap<std::string, std::string> headers;
    headers.insert(
        std::make_pair("Content-Type", "application/x-www-form-urlencoded"));

    // Create request body
    std::ostringstream request_body_stream;
    request_body_stream << "client_id=" << tokenOptions_.clientid
                        << "&client_secret=" << tokenOptions_.clientSecret
                        << "&scope=" << tokenOptions_.apiApplicationId
                        << "/.default"
                        << "&grant_type=client_credentials";
    std::string request_body = request_body_stream.str();

    // Make request
    return MakeRequest(tokenOptions_.endpoint,
                       google::scp::core::HttpMethod::POST, headers,
                       request_body);
}

}  // namespace privacy_sandbox::server_common