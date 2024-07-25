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

#include "azure_kms_client_provider_utils.h"

#include <memory>
#include <utility>

#include "src/azure/attestation/src/attestation.h"

#include "azure_kms_client_provider.h"

using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::Uri;
using google::scp::cpio::client_providers::AzureKmsClientProviderUtils;

namespace google::scp::cpio::client_providers {

/**
 * @brief Return true if input key is a private key
 */
bool AzureKmsClientProviderUtils::isPrivate(
    std::shared_ptr<EvpPkeyWrapper> key) {
  ERR_clear_error();
  // Determine if the key is private or public

  const int key_type = EVP_PKEY_type(EVP_PKEY_id(key->get()));
  bool is_private = false;
  if (key_type == EVP_PKEY_RSA) {
    RSA* rsa_raw = EVP_PKEY_get1_RSA(key->get());
    RsaWrapper rsa_wrapper(rsa_raw);
    if (rsa_raw != nullptr) {
      is_private = (rsa_wrapper.getE() != NULL && rsa_wrapper.getN() != NULL &&
                    rsa_wrapper.getD() != NULL);
    }
  }
  return is_private;
}

/**
 * @brief Generate hex hash on wrapping key
 */
absl::StatusOr<std::string> AzureKmsClientProviderUtils::CreateHexHashOnKey(
    std::shared_ptr<EvpPkeyWrapper> public_key) {
  ERR_clear_error();
  CHECK(isPrivate(public_key) == 0)
      << "CreateHexHashOnKey only supports public keys";

  // Create a BIO to hold the public key in PEM format
  BIOWrapper bio_wrapper;
  int result = PEM_write_bio_PUBKEY(bio_wrapper.get(), public_key->get());
  if (result != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(
        std::string("PEM_write_bio_PUBKEY failed with result: ") +
        std::to_string(result) + " - " + error_string);
  }

  // Read the PEM key into a string
  char* pem_key;
  const int64 pem_key_length = BIO_get_mem_data(bio_wrapper.get(), &pem_key);
  if (pem_key_length == -1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(std::string("BIO_get_mem_data failed: ") +
                               error_string);
  }
  if (pem_key == nullptr) {
    absl::InternalError("BIO_get_mem_data returned nullptr for pem_key");
  }
  const std::string pem_key_str(pem_key, pem_key_length);

  // Create a SHA-2 hash of the PEM key string
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_length;
  if (EVP_Digest(pem_key_str.c_str(), pem_key_str.size(), hash, &hash_length,
                 EVP_sha256(), NULL) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(std::string("Creating hash failed: ") +
                               std::string(error_string));
  }

  // Convert the hash to a hexadecimal string
  std::stringstream ss;
  for (unsigned int i = 0; i < hash_length; i++) {
    ss << std::hex << std::setw(2) << std::setfill('0')
       << static_cast<int>(hash[i]);
  }

  // Return the hexadecimal string
  return ss.str();
}

/**
 * @brief Generate a new wrapping key
 */
absl::StatusOr<
    std::pair<std::shared_ptr<EvpPkeyWrapper>, std::shared_ptr<EvpPkeyWrapper>>>
AzureKmsClientProviderUtils::GenerateWrappingKey() {
  try {
    RsaWrapper rsa;
    BnWrapper e;
    ERR_clear_error();
    if (!BN_set_word(e.get(), RSA_F4)) {
      char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(std::string("Failed to set RSA exponent: ") +
                                 std::string(error_string));
    }

    if (RSA_generate_key_ex(rsa.get(), 4096, e.get(), NULL) != 1) {
      char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(std::string("Failed to generate RSA key: ") +
                                 std::string(error_string));
    }

    // Check RSA key components
    if (rsa.getN() == nullptr || rsa.getE() == nullptr ||
        rsa.getD() == nullptr) {
      return absl::InternalError("RSA key components are not properly set");
    }

    std::shared_ptr<EvpPkeyWrapper> private_key =
        std::make_shared<EvpPkeyWrapper>(EVP_PKEY_new());
    if (!private_key->get()) {
      return absl::InternalError("Could not retrieve private_key memory");
    }

    if (EVP_PKEY_set1_RSA(private_key->get(), rsa.get()) != 1) {
      char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(
          std::string("Setting RSA key in private EVP_PKEY failed: ") +
          std::string(error_string));
    }

    // Duplicating the RSA public key
    RSA* rsa_pub = RSAPublicKey_dup(rsa.get());
    if (rsa_pub == nullptr) {
      char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(
          std::string("Duplicating RSA public key failed: ") +
          std::string(error_string));
    }

    // Creating a shared_ptr to manage the duplicated RSA public key
    std::shared_ptr<RsaWrapper> rsa_pub_dup =
        std::make_shared<RsaWrapper>(rsa_pub);

    // Creating a shared_ptr to manage the public key
    std::shared_ptr<EvpPkeyWrapper> public_key =
        std::make_shared<EvpPkeyWrapper>(EVP_PKEY_new());
    if (EVP_PKEY_set1_RSA(public_key->get(), rsa_pub_dup->get()) != 1) {
      char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(std::string("Set RSA public key failed: ") +
                                 std::string(error_string));
    }
    return std::make_pair(private_key, public_key);
  } catch (const std::exception& ex) {
    std::cerr << "Exception caught in GenerateWrappingKey: " << ex.what()
              << std::endl;
    throw;
  }
}

/**
 * @brief Convert a private PEM wrapping key to EVP_PKEY using private key
 *
 * @param wrapping_pem_key RSA PEM key used to wrap a key.
 * @return std::shared_ptr<EvpPkeyWrapper> containing the EVP_PKEY.
 */
absl::StatusOr<std::shared_ptr<EvpPkeyWrapper>>
AzureKmsClientProviderUtils::GetPrivateEvpPkey(std::string wrapping_pem_key) {
  ERR_clear_error();

  // Create a BIO wrapper to manage memory
  BIOWrapper bio_wrapper;

  // Ensure the bio is created successfully before using it
  if (bio_wrapper.get() == nullptr) {
    return absl::InternalError("Failed to create BIO");
  }

  // Write the PEM key data into the BIO
  if (BIO_write(bio_wrapper.get(), wrapping_pem_key.c_str(),
                wrapping_pem_key.size()) <= 0) {
    return absl::InternalError(
        "Failed to write PEM data to BIO in GetPrivateEvpPkey");
  }

  // Read the private key from the BIO
  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio_wrapper.get(), NULL, NULL, NULL);
  if (pkey == nullptr) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(
        std::string(
            "GetPrivateEvpPkey: Failed to read private key from BIO: ") +
        err_buffer);
  }

  // Use EvpPkeyWrapper to manage the EVP_PKEY
  return std::make_shared<EvpPkeyWrapper>(pkey);
}

/**
 * @brief Convert a PEM wrapping key to EVP_PKEY using public key
 *
 * @param wrapping_pem_key RSA PEM key used to wrap a key.
 * @return std::shared_ptr<EvpPkeyWrapper> containing the EVP_PKEY.
 */
absl::StatusOr<std::shared_ptr<EvpPkeyWrapper>>
AzureKmsClientProviderUtils::GetPublicEvpPkey(std::string wrapping_pem_key) {
  ERR_clear_error();

  // Create a BIO wrapper to manage memory
  BIOWrapper bio_wrapper;

  // Ensure the bio is created successfully before using it
  if (bio_wrapper.get() == nullptr) {
    return absl::InternalError("Failed to create BIO");
  }

  // Write the PEM key data into the BIO
  if (BIO_write(bio_wrapper.get(), wrapping_pem_key.c_str(),
                wrapping_pem_key.size()) <= 0) {
    return absl::InternalError(
        "Failed to write PEM data to BIO in GetPublicEvpPkey");
  }

  // Read the public key from the BIO
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio_wrapper.get(), NULL, NULL, NULL);
  if (pkey == nullptr) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(
        std::string("GetPublicEvpPkey: Failed to read PEM public key: ") +
        err_buffer);
  }

  // Return a shared pointer to manage the EVP_PKEY
  return std::make_shared<EvpPkeyWrapper>(pkey);
}

/**
 * @brief Convert a PEM wrapping key to pkey
 *
 * @param wrapping_pem_key RSA PEM key used to wrap a key.
 */
absl::StatusOr<std::shared_ptr<EvpPkeyWrapper>>
AzureKmsClientProviderUtils::PemToEvpPkey(std::string wrapping_pem_key) {
  ERR_clear_error();

  // check for public key or private key
  const bool isPrivate = wrapping_pem_key.find(kPemToken) != std::string::npos;
  std::shared_ptr<EvpPkeyWrapper> key;
  if (isPrivate) {
    const auto key_or = GetPrivateEvpPkey(wrapping_pem_key);
    if (!key_or.ok()) {
      return absl::InternalError(
          std::string("Failed to read private PEM key: ") +
          key_or.status().ToString().c_str());
    }
    key = key_or.value();
  } else {
    const auto key_or = GetPublicEvpPkey(wrapping_pem_key);
    if (!key_or.ok()) {
      return absl::InternalError(
          std::string("Failed to read private PEM key: ") +
          key_or.status().ToString().c_str());
    }
    key = key_or.value();
  }

  if (key->get() == NULL) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError(
        std::string("Failed to read private and public PEM key: ") +
        std::string(error_string));
  }

  // Wrap EVP_PKEY into std::shared_ptr<EvpPkeyWrapper>
  return key;
}

/**
 * @brief Convert a wrapping key to PEM
 *
 * @param wrapping_key RSA public key used to wrap a key.
 */
absl::StatusOr<std::string> AzureKmsClientProviderUtils::EvpPkeyToPem(
    std::shared_ptr<EvpPkeyWrapper> key) {
  ERR_clear_error();
  BIOWrapper bio_wrapper;

  BIO* bio = bio_wrapper.get();
  if (bio == nullptr) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError(std::string("Failed to create BIO: ") +
                               error_string);
  }

  const bool is_private = isPrivate(key);
  int write_result;
  if (is_private) {
    write_result = PEM_write_bio_PrivateKey(bio, key->get(), nullptr, nullptr,
                                            0, nullptr, nullptr);
  } else {
    write_result = PEM_write_bio_PUBKEY(bio, key->get());
  }

  if (write_result != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError(std::string("Failed to write PEM: ") +
                               error_string);
  }

  // BIO memory is managed by openssl
  BUF_MEM* bio_mem;
  BIO_get_mem_ptr(bio, &bio_mem);
  if (bio_mem == nullptr) {
    return absl::InternalError("Failed to get BIO memory pointer");
  }

  std::string pem_str(bio_mem->data, bio_mem->length);
  return pem_str;
}

/**
 * @brief Wrap a key using RSA OAEP
 *
 * @param wrapping_key RSA public key used to wrap a key.
 * @param key         Key  wrap.
 */
absl::StatusOr<std::vector<unsigned char>> AzureKmsClientProviderUtils::KeyWrap(
    std::shared_ptr<EvpPkeyWrapper> wrapping_key, const std::string& data) {
  ERR_clear_error();

  // Ensure that the wrapping key is public
  if (isPrivate(wrapping_key)) {
    ERR_clear_error();
    return absl::InternalError("Use public key for KeyWrap");
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrapping_key->get(), NULL));
  if (ctxWrapper.get() == nullptr) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to create EVP_PKEY_CTX: " +
                               std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for encryption
  if (EVP_PKEY_encrypt_init(ctx) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to initialize encryption context: " +
                               std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP padding: " +
                               std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP digest: " +
                               std::string(error_string));
  }

  // Get the maximum encrypted data size
  size_t encrypted_len;
  if (EVP_PKEY_encrypt(ctx, nullptr, &encrypted_len,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to get maximum encrypted data size: " +
                               std::string(error_string));
  }

  // Allocate space for the encrypted data
  std::vector<unsigned char> encrypted(encrypted_len);

  // Encrypt the data
  if (EVP_PKEY_encrypt(ctx, encrypted.data(), &encrypted_len,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Encryption failed: " +
                               std::string(error_string));
  }

  // Resize the encrypted data vector
  encrypted.resize(encrypted_len);
  return encrypted;
}

/**
 * @brief Unwrap a key using RSA OAEP
 *
 * @param wrapping_key RSA private key used to unwrap a key.
 * @param encrypted   Wrapped key to unwrap.
 */
absl::StatusOr<std::string> AzureKmsClientProviderUtils::KeyUnwrap(
    std::shared_ptr<EvpPkeyWrapper> wrapping_key,
    const std::vector<unsigned char>& encrypted) {
  ERR_clear_error();
  // Ensure that the wrapping key is private
  if (!isPrivate(wrapping_key)) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Use private key for KeyUnwrap: " +
                               std::string(error_string));
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrapping_key->get(), NULL));
  if (ctxWrapper.get() == nullptr) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to create EVP_PKEY_CTX: " +
                               std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for decryption
  if (EVP_PKEY_decrypt_init(ctx) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to initialize decryption context: " +
                               std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP padding: " +
                               std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP digest: " +
                               std::string(error_string));
  }

  // Get the maximum decrypted data size
  size_t decrypted_len;
  if (EVP_PKEY_decrypt(ctx, nullptr, &decrypted_len, encrypted.data(),
                       encrypted.size()) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to get maximum decrypted data size: " +
                               std::string(error_string));
  }

  // Allocate space for the decrypted data based on the maximum size
  std::vector<unsigned char> decrypted(decrypted_len);

  // Decrypt the data
  if (EVP_PKEY_decrypt(ctx, decrypted.data(), &decrypted_len, encrypted.data(),
                       encrypted.size()) != 1) {
    char err_buffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Decryption failed: " +
                               std::string(error_string));
  }

  // Resize the decrypted data vector and convert to string
  decrypted.resize(decrypted_len);
  return std::string(decrypted.begin(), decrypted.end());
}

}  // namespace google::scp::cpio::client_providers
