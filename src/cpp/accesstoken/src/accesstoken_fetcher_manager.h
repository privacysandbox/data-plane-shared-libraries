#ifndef SRC_CPP_ACCESSTOKEN_FETCHER_FACTORY_H_
#define SRC_CPP_ACCESSTOKEN_FETCHER_FACTORY_H_

#include <memory>

#include "public/core/interface/execution_result.h"

namespace privacy_sandbox::server_common {
using AccessTokenServiceEndpoint = std::string;
using ClientApplicationId = std::string;
using ClientSecret = std::string;
using ApiIdentifierUri = std::string;
using AccessTokenValue = std::string;

// Represents an accesstoken fetched from the token Service.
struct AccessToken {
  // The value of the accesstoken. This field is the raw, unencoded byte string
  AccessTokenValue accessToken;
};

/// Represents the accesstoken response object.
struct GetAccessTokenResponse {
  std::shared_ptr<std::string> accesstoken;
};
/// Represents the get accesstoken request object.
struct GetAccessTokenRequest {};

// Configuration for access token endpoint.
struct AccessTokenClientOptions {
  virtual ~AccessTokenClientOptions() = default;
  
  AccessTokenServiceEndpoint endpoint;
  ClientApplicationId clientid;
  ClientSecret clientSecret;
  ApiIdentifierUri apiUri;
};

class AccessTokenClientFactory {
 public:
  // Constructor that accepts AccessTokenClientOptions
  explicit AccessTokenClientFactory(
      const AccessTokenClientOptions& options) noexcept;

  static std::unique_ptr<AccessTokenClientFactory> Create(
      const AccessTokenClientOptions& options) noexcept;
  
  // Method to get access token
  std::string GetAccessToken();
  
 private:
  AccessTokenClientOptions options_;  // Member variable to store the options
};

}  // namespace server_common::privacy_sandbox

#endif  // SRC_CPP_ACCESSTOKEN_FETCHER_FACTORY_H_
