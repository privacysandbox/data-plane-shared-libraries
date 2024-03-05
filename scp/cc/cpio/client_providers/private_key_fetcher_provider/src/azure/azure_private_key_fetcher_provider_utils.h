/*
 * Portions Copyright (c) Microsoft Corporation
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

#ifndef CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_SRC_AZURE_AZURE_PRIVATE_KEY_FETCHER_PROVIDER_UTILS_H_
#define CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_SRC_AZURE_AZURE_PRIVATE_KEY_FETCHER_PROVIDER_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "cpio/client_providers/private_key_fetcher_provider/src/private_key_fetcher_provider.h"

namespace google::scp::cpio::client_providers {
class AzurePrivateKeyFetchingClientUtils {
 public:
  /**
   * @brief Create a Http Request object to query private key vending endpoint.
   *
   * @param private_key_fetching_request request to query private key.
   * @param http_request returned http request.
   */
  static void CreateHttpRequest(
      const PrivateKeyFetchingRequest& private_key_fetching_request,
      core::HttpRequest& http_request);

  /**
   * @brief Generate a new wrapping key
   */
  static RSA* GenerateWrappingKey();

  /**
   * @brief Wrap a key using RSA OAEP
   *
   * @param wrappingKey RSA public key used to wrap a key.
   * @param key         Key in PEM format to wrap.
   */
  static std::vector<unsigned char> KeyWrap(
      RSA* wrappingKey, const std::string& key);

  /**
   * @brief Unwrap a key using RSA OAEP
   *
   * @param wrappingKey RSA private key used to unwrap a key.
   * @param encrypted   Wrapped key to unwrap.
   */
  static std::string KeyUnwrap(
      RSA* wrappingKey, const std::vector<unsigned char>& encrypted);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROVIDERS_PRIVATE_KEY_FETCHER_PROVIDER_SRC_AZURE_AZURE_PRIVATE_KEY_FETCHER_PROVIDER_UTILS_H_
