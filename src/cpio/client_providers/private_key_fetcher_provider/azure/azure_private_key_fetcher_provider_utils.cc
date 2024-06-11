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

#include "azure_private_key_fetcher_provider_utils.h"

#include <memory>
#include <utility>

#include "src/azure/attestation/src/attestation.h"

using google::scp::azure::attestation::fetchFakeSnpAttestation;
using google::scp::azure::attestation::fetchSnpAttestation;
using google::scp::azure::attestation::hasSnp;
using google::scp::core::HttpMethod;
using google::scp::core::HttpRequest;
using google::scp::core::Uri;
using google::scp::cpio::client_providers::AzurePrivateKeyFetchingClientUtils;

namespace google::scp::cpio::client_providers {

bool AzurePrivateKeyFetchingClientUtils::isPrivate(EVP_PKEY* pkey) {
  // Determine if the key is private or public
  int key_type = EVP_PKEY_type(pkey->type);
  bool is_private = false;
  if (key_type == EVP_PKEY_RSA) {
    const RSA* rsa_key = EVP_PKEY_get1_RSA(pkey);
    is_private =
        (rsa_key->e != NULL && rsa_key->n != NULL && rsa_key->d != NULL);
  }
  return is_private;
}

void AzurePrivateKeyFetchingClientUtils::CreateHttpRequest(
    const PrivateKeyFetchingRequest& request, HttpRequest& http_request) {
  const auto& base_uri =
      request.key_vending_endpoint->private_key_vending_service_endpoint;
  http_request.method = HttpMethod::POST;

  http_request.path = std::make_shared<Uri>(base_uri);

  // Get Attestation Report
  const auto report =
      hasSnp() ? fetchSnpAttestation("") : fetchFakeSnpAttestation();
  CHECK(report.has_value()) << "Failed to get attestation report";

  nlohmann::json json_obj;
  json_obj[kAttestation] = report.value();

  http_request.body = core::BytesBuffer(json_obj.dump());
}

/**
 * @brief Generate hex hash on wrapping key
 */
std::string AzurePrivateKeyFetchingClientUtils::CreateHexHashOnKey(
    EVP_PKEY* publicKey) {
  // Create a BIO to hold the public key in PEM format
  BIOWrapper bioWrapper(const_cast<BIO_METHOD*>(BIO_s_mem()));
  PEM_write_bio_PUBKEY(bioWrapper.get(), publicKey);

  // Read the PEM key into a string
  char* pem_key;
  int64 pem_key_length = BIO_get_mem_data(bioWrapper.get(), &pem_key);
  std::string pem_key_str(pem_key, pem_key_length);

  // Create a SHA-2 hash of the PEM key string
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_length;
  if (EVP_Digest(pem_key_str.c_str(), pem_key_str.size(), hash, &hash_length,
                 EVP_sha256(), NULL) != 1) {
    char* error_string = ERR_error_string(ERR_get_error(), NULL);
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
AzurePrivateKeyFetchingClientUtils::GenerateWrappingKey() {
  RsaWrapper rsa;
  BnWrapper e;
  ERR_clear_error();
  BN_set_word(e.get(), RSA_F4);

  RSA_generate_key_ex(rsa.get(), 4096, e.get(), NULL);

  std::shared_ptr<EvpPkeyWrapper> private_key =
      std::make_shared<EvpPkeyWrapper>();
  if (EVP_PKEY_set1_RSA(private_key->get(), rsa.get()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Getting private EVP_PKEY failed: ") +
                             std::string(error_string));
  }

  // Create a new EVP_PKEY for the public key
  const RSA* rsa_pub = EVP_PKEY_get1_RSA(private_key->get());
  RsaWrapper rsa_pub_dup;
  if (!RSA_set0_key(rsa_pub_dup.get(), rsa_pub->n, rsa_pub->e, NULL)) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(
        std::string("Set RSA public key duplicate values failed: ") +
        std::string(error_string));
  }
  RSA_up_ref(rsa_pub_dup.get());
  std::shared_ptr<EvpPkeyWrapper> public_key =
      std::make_shared<EvpPkeyWrapper>();
  if (EVP_PKEY_set1_RSA(public_key->get(), rsa_pub_dup.get()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Set RSA public key failed: ") +
                             std::string(error_string));
  }

  return std::make_pair(private_key, public_key);
}

/**
 * @brief Convert a private PEM wrapping key to pkey
 *
 * @param wrappingPemKey RSA PEM key used to wrap a key.
 */
EVP_PKEY* AzurePrivateKeyFetchingClientUtils::GetPrivateEvpPkey(
    std::string wrappingPemKey) {
  ERR_clear_error();
  // Create a BIOWrapper object to manage the BIO resource
  BIOWrapper bioWrapper(const_cast<BIO_METHOD*>(BIO_s_mem()));

  BIO* bio = bioWrapper.get();
  if (bio == NULL) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to create BIO: ") +
                             std::string(error_string));
  }

  // Write PEM data to BIO
  if (BIO_write(bio, wrappingPemKey.c_str(), wrappingPemKey.size()) <= 0) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to write to BIO: ") +
                             std::string(error_string));
  }

  // Read the PEM key
  EVP_PKEY* pkey = PEM_read_bio_PrivateKey(bio, NULL, NULL, NULL);
  return pkey;
}

/**
 * @brief Convert a public PEM wrapping key to pkey
 *
 * @param wrappingPemKey RSA PEM key used to wrap a key.
 */
EVP_PKEY* AzurePrivateKeyFetchingClientUtils::GetPublicEvpPkey(
    std::string wrappingPemKey) {
  ERR_clear_error();
  // Create a BIOWrapper object to manage the BIO resource
  BIOWrapper bioWrapper(const_cast<BIO_METHOD*>(BIO_s_mem()));

  BIO* bio = bioWrapper.get();
  if (bio == NULL) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to create BIO: ") +
                             std::string(error_string));
  }

  // Write PEM data to BIO
  if (BIO_write(bio, wrappingPemKey.c_str(), wrappingPemKey.size()) <= 0) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to write to BIO: ") +
                             std::string(error_string));
  }

  // Read the PEM key
  EVP_PKEY* pkey = PEM_read_bio_PUBKEY(bio, NULL, NULL, NULL);
  return pkey;
}

/**
 * @brief Convert a PEM wrapping key to pkey
 *
 * @param wrappingPemKey RSA PEM key used to wrap a key.
 */
EVP_PKEY* AzurePrivateKeyFetchingClientUtils::PemToEvpPkey(
    std::string wrappingPemKey) {
  ERR_clear_error();
  EVP_PKEY* pkey = GetPrivateEvpPkey(wrappingPemKey);
  if (pkey == NULL) {
    // Attempt to read the PEM key as a public key
    pkey = GetPublicEvpPkey(wrappingPemKey);
  }

  if (pkey == NULL) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(
        std::string("Failed to read private and public PEM key: ") +
        std::string(error_string));
  }

  return pkey;
}

/**
 * @brief Convert a wrapping key to PEM
 *
 * @param wrappingKey RSA public key used to wrap a key.
 */
std::string AzurePrivateKeyFetchingClientUtils::EvpPkeyToPem(EVP_PKEY* pkey) {
  ERR_clear_error();
  // Create a BIOWrapper object to manage the BIO resource
  BIOWrapper bioWrapper(const_cast<BIO_METHOD*>(BIO_s_mem()));

  // Get the BIO object from the wrapper
  BIO* bio = bioWrapper.get();
  if (bio == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to create BIO: ") +
                             std::string(error_string));
  }

  // Determine if the key is private or public
  bool is_private = isPrivate(pkey);

  // Write the key to the BIO as PEM
  int write_result;
  if (is_private) {
    write_result = PEM_write_bio_PrivateKey(bio, pkey, nullptr, nullptr, 0,
                                            nullptr, nullptr);
  } else {
    write_result = PEM_write_bio_PUBKEY(bio, pkey);
  }

  if (write_result != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error(std::string("Failed to write PEM: ") +
                             std::string(error_string));
  }

  // Get the PEM string from the BIO
  BUF_MEM* bio_mem;
  BIO_get_mem_ptr(bio, &bio_mem);
  std::string pem_str(bio_mem->data, bio_mem->length);

  // No need to free the BIO explicitly, as it will be automatically
  // cleaned up when the BIOWrapper object goes out of scope

  return pem_str;
}

/**
 * @brief Wrap a key using RSA OAEP
 *
 * @param wrappingKey RSA public key used to wrap a key.
 * @param key         Key  wrap.
 */
std::vector<unsigned char> AzurePrivateKeyFetchingClientUtils::KeyWrap(
    EVP_PKEY* wrappingKey, const std::string& data) {
  ERR_clear_error();

  // Ensure that the wrapping key is public
  if (isPrivate(wrappingKey)) {
    ERR_clear_error();
    throw std::runtime_error("Use public key for KeyWrap");
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrappingKey, NULL));
  if (ctxWrapper.get() == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to create EVP_PKEY_CTX: " +
                             std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for encryption
  if (EVP_PKEY_encrypt_init(ctx) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to initialize encryption context: " +
                             std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP padding: " +
                             std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
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
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
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
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
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
std::string AzurePrivateKeyFetchingClientUtils::KeyUnwrap(
    EVP_PKEY* wrappingKey, const std::vector<unsigned char>& encrypted) {
  ERR_clear_error();
  // Ensure that the wrapping key is private
  if (!isPrivate(wrappingKey)) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Use private key for KeyUnwrap: " +
                             std::string(error_string));
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrappingKey, NULL));
  if (ctxWrapper.get() == nullptr) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to create EVP_PKEY_CTX: " +
                             std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for decryption
  if (EVP_PKEY_decrypt_init(ctx) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to initialize decryption context: " +
                             std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP padding: " +
                             std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Failed to set OAEP digest: " +
                             std::string(error_string));
  }

  // Get the maximum decrypted data size
  size_t decrypted_len;
  if (EVP_PKEY_decrypt(ctx, nullptr, &decrypted_len, encrypted.data(),
                       encrypted.size()) != 1) {
    char errBuffer[MAX_OPENSSL_ERROR_STRING_LEN];
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
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
    char* error_string = ERR_error_string(ERR_get_error(), errBuffer);
    ERR_clear_error();
    throw std::runtime_error("Decryption failed: " + std::string(error_string));
  }

  // Resize the decrypted data vector and convert to string
  decrypted.resize(decrypted_len);
  return std::string(decrypted.begin(), decrypted.end());
}

}  // namespace google::scp::cpio::client_providers
