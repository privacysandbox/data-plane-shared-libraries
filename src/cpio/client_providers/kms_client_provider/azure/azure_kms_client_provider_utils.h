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

#ifndef CPIO_CLIENT_PROV_AZURE_KMS_CLIENT_PROVIDER_UTILS_H_
#define CPIO_CLIENT_PROV_AZURE_KMS_CLIENT_PROVIDER_UTILS_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

#include "absl/log/check.h"
#include "absl/status/statusor.h"

namespace google::scp::cpio::client_providers {

/*
 * What is the wrapping key?
 * The wrapping key is an RSA public key used by the /unwrapkey KMS endpoint.
 * When making a request to this endpoint, the caller must provide this RSA
 * public key. The /unwrapkey endpoint uses the provided public key to encrypt
 * the plaintext HPKE private key which is returned in the respone. This ensures
 * that the HPKE private key remains encrypted while traveling over proxies
 * between KMS and B&A services. The caller can later use its corresponding
 * private key to decrypt and obtain the actual plaintext HPKE private key.
 *
 * Security:
 * To prevent man-in-the-middle attacks, the KMS /unwrapkey endpoint validates
 * that the presented wrapping key is owned by the calling service. This
 * validation is done by checking the report data in the attestation report.
 *
 * Why do we use a test wrapping key in non-production environments?
 * In production environments on real SNP hardware, the wrapping key is
 * generated dynamically. A hash of the key is included in the attestation
 * report (report data). In test environments, we cannot generate real-time
 * attestation reports. Instead, we use a fake attestation report that hardcodes
 * the hash of a test wrapping key.
 */

// Define RAII memory allocation/deallocation classes
class RsaWrapper {
 public:
  RsaWrapper() : rsa_(RSA_new()) {}
  explicit RsaWrapper(RSA* rsa_raw) : rsa_(rsa_raw) {}

  ~RsaWrapper() { RSA_free(rsa_); }

  RSA* get() { return rsa_; }

  const BIGNUM* getE() const { return rsa_->e; }
  const BIGNUM* getN() const { return rsa_->n; }
  const BIGNUM* getD() const { return rsa_->d; }

 private:
  RSA* rsa_;
};

class BnWrapper {
 public:
  BnWrapper() : bn_(BN_new()) {}
  explicit BnWrapper(BIGNUM* bn) : bn_(bn) {}
  ~BnWrapper() { BN_free(bn_); }

  BIGNUM* get() { return bn_; }

 private:
  BIGNUM* bn_;
};

class EvpPkeyWrapper {
 public:
  // Constructor accepting an EVP_PKEY*
  explicit EvpPkeyWrapper(EVP_PKEY* pkey) : pkey_(pkey) {}

  // Default constructor
  EvpPkeyWrapper() : pkey_(nullptr) {}

  // Destructor
  ~EvpPkeyWrapper() {
    if (pkey_) {
      EVP_PKEY_free(pkey_);
    }
  }

  // Getter for the EVP_PKEY* pointer
  EVP_PKEY* get() const { return pkey_; }

 private:
  EVP_PKEY* pkey_;
};

class BIOWrapper {
 public:
  BIOWrapper() : bio_(BIO_new(BIO_s_mem())) {
    CHECK(bio_) << "Failed to create BIO";
  }

  ~BIOWrapper() { BIO_free(bio_); }

  BIO* get() { return bio_; }

 private:
  BIO* bio_;
};

class EVPKeyCtxWrapper {
 public:
  explicit EVPKeyCtxWrapper(EVP_PKEY_CTX* ctx) : ctx_(ctx) {}

  ~EVPKeyCtxWrapper() {
    if (ctx_) {
      EVP_PKEY_CTX_free(ctx_);
      ctx_ = nullptr;
    }
  }

  EVP_PKEY_CTX* get() const { return ctx_; }

 private:
  EVP_PKEY_CTX* ctx_;
};

class AzureKmsClientProviderUtils {
 public:
  /**
   * @brief Generate a new wrapping key
   */
  static absl::StatusOr<std::pair<std::shared_ptr<EvpPkeyWrapper>,
                                  std::shared_ptr<EvpPkeyWrapper>>>
  GenerateWrappingKey();

  /**
   * @brief Convert a wrapping key in PEM
   *
   * @param wrapping_key RSA public key used to wrap a key.
   */
  static absl::StatusOr<std::string> EvpPkeyToPem(
      std::shared_ptr<EvpPkeyWrapper> wrapping_key);

  /**
   * @brief Generate hex hash on wrapping key
   */
  static absl::StatusOr<std::string> CreateHexHashOnKey(
      std::shared_ptr<EvpPkeyWrapper> public_key);

  /**
   * @brief Wrap a key using RSA OAEP
   *
   * @param wrapping_key RSA public key used to wrap a key.
   * @param key         Key in PEM format to wrap.
   */
  static absl::StatusOr<std::vector<unsigned char>> KeyWrap(
      std::shared_ptr<EvpPkeyWrapper> wrapping_key, const std::string& key);

  /**
   * @brief Unwrap a key using RSA OAEP
   *
   * @param wrapping_key RSA private key used to unwrap a key.
   * @param encrypted   Wrapped key to unwrap.
   */
  static absl::StatusOr<std::string> KeyUnwrap(
      std::shared_ptr<EvpPkeyWrapper> wrapping_key,
      const std::vector<unsigned char>& encrypted);

  // Declare the isPrivate function as private
  static bool isPrivate(std::shared_ptr<EvpPkeyWrapper> key);

  /**
   * @brief Convert a PEM wrapping key to pkey
   *
   * @param wrapping_pem_key RSA PEM key used to wrap a key.
   */
  static absl::StatusOr<std::shared_ptr<EvpPkeyWrapper>> PemToEvpPkey(
      std::string wrapping_pem_key);

  /**
   * @brief Return public wrapping key for test
   */
  static std::string GetTestPemPublicWrapKey();

  /**
   * @brief Return private wrapping key for test
   */
  static std::string GetTestPemPrivWrapKey();

 private:
  /**
   * @brief Convert a public PEM wrapping key to pkey
   *
   * @param wrapping_pem_key RSA PEM key used to wrap a key.
   */
  static absl::StatusOr<std::shared_ptr<EvpPkeyWrapper>> GetPublicEvpPkey(
      std::string wrapping_pem_key);

  /**
   * @brief Convert a private PEM wrapping key to pkey
   *
   * @param wrapping_pem_key RSA PEM key used to wrap a key.
   */
  static absl::StatusOr<std::shared_ptr<EvpPkeyWrapper>> GetPrivateEvpPkey(
      std::string wrapping_pem_key);
};
}  // namespace google::scp::cpio::client_providers

#endif  // CPIO_CLIENT_PROV_AZURE_KMS_CLIENT_PROVIDER_UTILS_H_
