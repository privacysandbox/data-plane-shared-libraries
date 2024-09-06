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

using google::scp::cpio::client_providers::AzureKmsClientProviderUtils;

namespace {

constexpr unsigned int kMaxOpensslErrorStringLen = 256;

constexpr char kPemSeperator[] = "-----";
constexpr char kPemEnd[] = "END ";
constexpr char kPemToken[] = "PRIVATE ";
constexpr char kPemKey[] = "KEY";
constexpr char kPemBegin[] = "BEGIN ";

constexpr char kWrappingKp[] = R"(
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
/Kg7pJF1e2hkk3vxtCSlu9HfZ4q17vg=
)";
}  // namespace

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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(
        std::string("PEM_write_bio_PUBKEY failed with result: ") +
        std::to_string(result) + " - " + error_string);
  }

  // Read the PEM key into a string
  char* pem_key;
  const auto pem_key_length = BIO_get_mem_data(bio_wrapper.get(), &pem_key);
  if (pem_key_length == -1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    return absl::InternalError(std::string("BIO_get_mem_data failed: ") +
                               error_string);
  }
  if (pem_key == nullptr) {
    return absl::InternalError("BIO_get_mem_data returned nullptr for pem_key");
  }
  const std::string pem_key_str(pem_key, pem_key_length);

  // Create a SHA-2 hash of the PEM key string
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hash_length;
  if (EVP_Digest(pem_key_str.c_str(), pem_key_str.size(), hash, &hash_length,
                 EVP_sha256(), NULL) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
      char err_buffer[kMaxOpensslErrorStringLen];
      char* error_string = nullptr;
      error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(std::string("Failed to set RSA exponent: ") +
                                 std::string(error_string));
    }

    if (RSA_generate_key_ex(rsa.get(), 4096, e.get(), NULL) != 1) {
      char err_buffer[kMaxOpensslErrorStringLen];
      char* error_string = nullptr;
      error_string =
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
      char err_buffer[kMaxOpensslErrorStringLen];
      char* error_string = nullptr;
      error_string =
          ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
      return absl::InternalError(
          std::string("Setting RSA key in private EVP_PKEY failed: ") +
          std::string(error_string));
    }

    // Duplicating the RSA public key
    RSA* rsa_pub = RSAPublicKey_dup(rsa.get());
    if (rsa_pub == nullptr) {
      char err_buffer[kMaxOpensslErrorStringLen];
      char* error_string = nullptr;
      error_string =
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
      char err_buffer[kMaxOpensslErrorStringLen];
      char* error_string = nullptr;
      error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to create EVP_PKEY_CTX: " +
                               std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for encryption
  if (EVP_PKEY_encrypt_init(ctx) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to initialize encryption context: " +
                               std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP padding: " +
                               std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Use private key for KeyUnwrap: " +
                               std::string(error_string));
  }

  // Create a wrapper for the EVP_PKEY_CTX resource
  EVPKeyCtxWrapper ctxWrapper(EVP_PKEY_CTX_new(wrapping_key->get(), NULL));
  if (ctxWrapper.get() == nullptr) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to create EVP_PKEY_CTX: " +
                               std::string(error_string));
  }

  EVP_PKEY_CTX* ctx = ctxWrapper.get();

  // Initialize the context for decryption
  if (EVP_PKEY_decrypt_init(ctx) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to initialize decryption context: " +
                               std::string(error_string));
  }

  // Set the OAEP padding
  if (EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP padding: " +
                               std::string(error_string));
  }

  // Set the OAEP parameters
  const EVP_MD* md = EVP_sha256();
  if (EVP_PKEY_CTX_set_rsa_oaep_md(ctx, md) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Failed to set OAEP digest: " +
                               std::string(error_string));
  }

  // Get the maximum decrypted data size
  size_t decrypted_len;
  if (EVP_PKEY_decrypt(ctx, nullptr, &decrypted_len, encrypted.data(),
                       encrypted.size()) != 1) {
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
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
    char err_buffer[kMaxOpensslErrorStringLen];
    char* error_string = nullptr;
    error_string =
        ERR_error_string_n(ERR_get_error(), error_string, sizeof(err_buffer));
    ERR_clear_error();
    return absl::InternalError("Decryption failed: " +
                               std::string(error_string));
  }

  // Resize the decrypted data vector and convert to string
  decrypted.resize(decrypted_len);
  return std::string(decrypted.begin(), decrypted.end());
}

std::string AzureKmsClientProviderUtils::GetTestPemPublicWrapKey() {
  return R"(
-----BEGIN PUBLIC KEY-----
MIICIjANBgkqhkiG9w0BAQEFAAOCAg8AMIICCgKCAgEA0L9FDBjydkdstv7OKqkw
dMiugRqlSHC9Lchfd7jh5uCzv602LhlBJQeEFYchvaEquISLQFoZxEkpGEbEb15v
N2dKwTCi0ioEGBidtFmuKiVZqf46Hbnw4OdinQHrlGO2PRsRE+DYPOy6xrZTKEnD
K+OnHDsZ0U2qNJ80IjbxcC83lQpaMx8Ij8AddGY9Msv0TMMgaVsrDaQYLC8tmJih
xVI2f5BHFbTy3pw4Sq9xp6se+S/ycOUF0M6RFArZcD9uR/NxJKFJJlGOKskgwLnT
lhPck4sGeLhHUydRdqw0+h8EDsTIUysNxMOYtZETaVuBS+ISMy8+WQgVPs12Ujr3
17kaHeZr8Lq0bwaFHruRBpNwqtUCBv57IBpe7hnEDDdvOvN/tPubf1dv3HxEt42T
qqfozS+a9+1hcI8hpNlEjh+qcy1BmhSOXvmRzlhauX4xv6OLCNkRxo6x2Q/1moDC
PgnaJJVIuESr07xnC8fk43i5qFzEXQO3hwNsjd7sqFBBTb6t5N6Nm37mMNTqsDky
niAeFG+1gK/UD+cMPfbUIDaCqpCDwrTX0gMqqUTDG6eNPmQaOa+slici3h9WLaNy
mEzfKqJMBggKib/+e4Eb/ENdvxeT1X2YXpZ3tjZE+bRoiDgN4FYqBzYtZ/ieRcsq
4fPqgZPbh+ivT2o7QutzWH0CAwEAAQ==
-----END PUBLIC KEY-----

)";
}

std::string AzureKmsClientProviderUtils::GetTestPemPrivWrapKey() {
  std::string result = std::string(kPemSeperator) + kPemBegin + kPemToken +
                       kPemKey + kPemSeperator + "\n" + kWrappingKp +
                       kPemSeperator + kPemEnd + kPemToken + kPemKey +
                       kPemSeperator + "\n";
  return result;
}

}  // namespace google::scp::cpio::client_providers
