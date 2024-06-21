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

  int key_type = EVP_PKEY_type(EVP_PKEY_id(key->get()));
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
std::string AzureKmsClientProviderUtils::CreateHexHashOnKey(
    std::shared_ptr<EvpPkeyWrapper> publicKey) {
  ERR_clear_error();
  CHECK(isPrivate(publicKey) == 0)
      << "CreateHexHashOnKey only supports public keys";

  // Create a BIO to hold the public key in PEM format
  BIOWrapper bioWrapper;
  int result = PEM_write_bio_PUBKEY(bioWrapper.get(), publicKey->get());
  if (result != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    throw std::runtime_error(
        std::string("PEM_write_bio_PUBKEY failed with result: ") +
        std::to_string(result) + " - " + error_string);
  }

  // Read the PEM key into a string
  char* pem_key;
  int64 pem_key_length = BIO_get_mem_data(bioWrapper.get(), &pem_key);
  if (pem_key_length == -1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    throw std::runtime_error(std::string("BIO_get_mem_data failed: ") +
                             error_string);
  }
  if (pem_key == nullptr) {
    throw std::runtime_error("BIO_get_mem_data returned nullptr for pem_key");
  }
  std::string pem_key_str(pem_key, pem_key_length);

  // Create a SHA-2 hash of the PEM key string
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_length;
  if (EVP_Digest(pem_key_str.c_str(), pem_key_str.size(), hash, &hash_length,
                 EVP_sha256(), NULL) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    throw std::runtime_error(std::string("Creating hash failed: ") +
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
std::pair<std::shared_ptr<EvpPkeyWrapper>, std::shared_ptr<EvpPkeyWrapper>>
AzureKmsClientProviderUtils::GenerateWrappingKey() {
  try {
    RsaWrapper rsa;
    BnWrapper e;
    ERR_clear_error();
    if (!BN_set_word(e.get(), RSA_F4)) {
      char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
      throw std::runtime_error(std::string("Failed to set RSA exponent: ") +
                               std::string(error_string));
    }

    if (RSA_generate_key_ex(rsa.get(), 4096, e.get(), NULL) != 1) {
      char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
      throw std::runtime_error(std::string("Failed to generate RSA key: ") +
                               std::string(error_string));
    }

    // Check RSA key components
    if (rsa.getN() == nullptr || rsa.getE() == nullptr ||
        rsa.getD() == nullptr) {
      throw std::runtime_error("RSA key components are not properly set");
    }

    std::shared_ptr<EvpPkeyWrapper> private_key =
        std::make_shared<EvpPkeyWrapper>(EVP_PKEY_new());
    if (!private_key->get()) {
      throw std::runtime_error("Could not retrieve private_key memory");
    }

    if (EVP_PKEY_set1_RSA(private_key->get(), rsa.get()) != 1) {
      char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
      throw std::runtime_error(
          std::string("Setting RSA key in private EVP_PKEY failed: ") +
          std::string(error_string));
    }

    // Duplicating the RSA public key
    RSA* rsa_pub = RSAPublicKey_dup(rsa.get());
    if (rsa_pub == nullptr) {
      char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
      throw std::runtime_error(
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
      char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
      char* error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
      throw std::runtime_error(std::string("Set RSA public key failed: ") +
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
 * @param wrappingPemKey RSA PEM key used to wrap a key.
 * @return std::shared_ptr<EvpPkeyWrapper> containing the EVP_PKEY.
 */
std::shared_ptr<EvpPkeyWrapper> AzureKmsClientProviderUtils::GetPrivateEvpPkey(
    std::string wrappingPemKey) {
  ERR_clear_error();

  // Create a BIO wrapper to manage memory
  BIOWrapper bioWrapper;

  // Ensure the bio is created successfully before using it
  if (bioWrapper.get() == nullptr) {
    throw std::runtime_error("Failed to create BIO");
  }

  // Write the PEM key data into the BIO
  if (BIO_write(bioWrapper.get(), wrappingPemKey.c_str(),
                wrappingPemKey.size()) <= 0) {
    throw std::runtime_error(
        "Failed to write PEM data to BIO in GetPrivateEvpPkey");
  }

  // Read the private key from the BIO
  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bioWrapper.get(), NULL, NULL, NULL);
  if (pkey == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    throw std::runtime_error(
        std::string(
            "GetPrivateEvpPkey: Failed to read private key from BIO: ") +
        errBuffer);
  }

  // Use EvpPkeyWrapper to manage the EVP_PKEY
  return std::make_shared<EvpPkeyWrapper>(pkey);
}

/**
 * @brief Convert a PEM wrapping key to EVP_PKEY using public key
 *
 * @param wrappingPemKey RSA PEM key used to wrap a key.
 * @return std::shared_ptr<EvpPkeyWrapper> containing the EVP_PKEY.
 */
std::shared_ptr<EvpPkeyWrapper> AzureKmsClientProviderUtils::GetPublicEvpPkey(
    std::string wrappingPemKey) {
  ERR_clear_error();

  // Create a BIO wrapper to manage memory
  BIOWrapper bioWrapper;

  // Ensure the bio is created successfully before using it
  if (bioWrapper.get() == nullptr) {
    throw std::runtime_error("Failed to create BIO");
  }

  // Write the PEM key data into the BIO
  if (BIO_write(bioWrapper.get(), wrappingPemKey.c_str(),
                wrappingPemKey.size()) <= 0) {
    throw std::runtime_error(
        "Failed to write PEM data to BIO in GetPublicEvpPkey");
  }

  // Read the public key from the BIO
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bioWrapper.get(), NULL, NULL, NULL);
  if (pkey == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    throw std::runtime_error(
        std::string("GetPublicEvpPkey: Failed to read PEM public key: ") +
        errBuffer);
  }

  // Return a shared pointer to manage the EVP_PKEY
  return std::make_shared<EvpPkeyWrapper>(pkey);
}

/**
 * @brief Convert a PEM wrapping key to pkey
 *
 * @param wrappingPemKey RSA PEM key used to wrap a key.
 */
std::shared_ptr<EvpPkeyWrapper> AzureKmsClientProviderUtils::PemToEvpPkey(
    std::string wrappingPemKey) {
  ERR_clear_error();

  // check for public key or private key
  bool isPrivate = wrappingPemKey.find(kPemToken) != std::string::npos;
  std::shared_ptr<EvpPkeyWrapper> key;
  if (isPrivate) {
    key = GetPrivateEvpPkey(wrappingPemKey);
  } else {
    key = GetPublicEvpPkey(wrappingPemKey);
  }

  if (key->get() == NULL) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error(
        std::string("Failed to read private and public PEM key: ") +
        std::string(error_string));
  }

  // Wrap EVP_PKEY into std::shared_ptr<EvpPkeyWrapper>
  return key;
}

/**
 * @brief Convert a wrapping key to PEM
 *
 * @param wrappingKey RSA public key used to wrap a key.
 */
std::string AzureKmsClientProviderUtils::EvpPkeyToPem(
    std::shared_ptr<EvpPkeyWrapper> key) {
  ERR_clear_error();
  BIOWrapper bioWrapper;

  BIO* bio = bioWrapper.get();
  if (bio == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to create BIO: ") +
                             error_string);
  }

  bool is_private = isPrivate(key);
  int write_result;
  if (is_private) {
    write_result = PEM_write_bio_PrivateKey(bio, key->get(), nullptr, nullptr,
                                            0, nullptr, nullptr);
  } else {
    write_result = PEM_write_bio_PUBKEY(bio, key->get());
  }

  if (write_result != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to write PEM: ") +
                             error_string);
  }

  // BIO memory is managed by openssl
  BUF_MEM* bio_mem;
  BIO_get_mem_ptr(bio, &bio_mem);
  if (bio_mem == nullptr) {
    throw std::runtime_error("Failed to get BIO memory pointer");
  }

  std::string pem_str(bio_mem->data, bio_mem->length);
  return pem_str;
}

/**
 * @brief Wrap a key using RSA OAEP
 *
 * @param wrappingKey RSA public key used to wrap a key.
 * @param key         Key  wrap.
 */
std::vector<unsigned char> AzureKmsClientProviderUtils::KeyWrap(
    std::shared_ptr<EvpPkeyWrapper> wrappingKey, const std::string& data) {
  ERR_clear_error();

  // Ensure that the wrapping key is public
  if (isPrivate(wrappingKey)) {
    ERR_clear_error();
    throw std::runtime_error("Use public key for KeyWrap");
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrappingKey->get(), NULL));
  if (ctxWrapper.get() == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to create EVP_PKEY_CTX: " +
                             std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for encryption
  if (EVP_PKEY_encrypt_init(ctx) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to initialize encryption context: " +
                             std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP padding: " +
                             std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP digest: " +
                             std::string(error_string));
  }

  // Get the maximum encrypted data size
  size_t encrypted_len;
  if (EVP_PKEY_encrypt(ctx, nullptr, &encrypted_len,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to get maximum encrypted data size: " +
                             std::string(error_string));
  }

  // Allocate space for the encrypted data
  std::vector<unsigned char> encrypted(encrypted_len);

  // Encrypt the data
  if (EVP_PKEY_encrypt(ctx, encrypted.data(), &encrypted_len,
                       reinterpret_cast<const unsigned char*>(data.data()),
                       data.size()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Encryption failed: " + std::string(error_string));
  }

  // Resize the encrypted data vector
  encrypted.resize(encrypted_len);
  return encrypted;
}

/**
 * @brief Unwrap a key using RSA OAEP
 *
 * @param wrappingKey RSA private key used to unwrap a key.
 * @param encrypted   Wrapped key to unwrap.
 */
std::string AzureKmsClientProviderUtils::KeyUnwrap(
    std::shared_ptr<EvpPkeyWrapper> wrappingKey,
    const std::vector<unsigned char>& encrypted) {
  ERR_clear_error();
  // Ensure that the wrapping key is private
  if (!isPrivate(wrappingKey)) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Use private key for KeyUnwrap: " +
                             std::string(error_string));
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrappingKey->get(), NULL));
  if (ctxWrapper.get() == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to create EVP_PKEY_CTX: " +
                             std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for decryption
  if (EVP_PKEY_decrypt_init(ctx) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to initialize decryption context: " +
                             std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP padding: " +
                             std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP digest: " +
                             std::string(error_string));
  }

  // Get the maximum decrypted data size
  size_t decrypted_len;
  if (EVP_PKEY_decrypt(ctx, nullptr, &decrypted_len, encrypted.data(),
                       encrypted.size()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Failed to get maximum decrypted data size: " +
                             std::string(error_string));
  }

  // Allocate space for the decrypted data based on the maximum size
  std::vector<unsigned char> decrypted(decrypted_len);

  // Decrypt the data
  if (EVP_PKEY_decrypt(ctx, decrypted.data(), &decrypted_len, encrypted.data(),
                       encrypted.size()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(errBuffer));
    ERR_clear_error();
    throw std::runtime_error("Decryption failed: " + std::string(error_string));
  }

  // Resize the decrypted data vector and convert to string
  decrypted.resize(decrypted_len);
  return std::string(decrypted.begin(), decrypted.end());
}

}  // namespace google::scp::cpio::client_providers
