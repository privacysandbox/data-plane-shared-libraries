# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/proto/crypto_service/v1/crypto_service.proto](#src_public_cpio_proto_crypto_service_v1_crypto_service-proto)
  - [AeadDecryptRequest](#google-cmrt-sdk-crypto_service-v1-AeadDecryptRequest)
  - [AeadDecryptResponse](#google-cmrt-sdk-crypto_service-v1-AeadDecryptResponse)
  - [AeadEncryptRequest](#google-cmrt-sdk-crypto_service-v1-AeadEncryptRequest)
  - [AeadEncryptResponse](#google-cmrt-sdk-crypto_service-v1-AeadEncryptResponse)
  - [AeadEncryptedData](#google-cmrt-sdk-crypto_service-v1-AeadEncryptedData)
  - [HpkeDecryptRequest](#google-cmrt-sdk-crypto_service-v1-HpkeDecryptRequest)
  - [HpkeDecryptResponse](#google-cmrt-sdk-crypto_service-v1-HpkeDecryptResponse)
  - [HpkeEncryptRequest](#google-cmrt-sdk-crypto_service-v1-HpkeEncryptRequest)
  - [HpkeEncryptResponse](#google-cmrt-sdk-crypto_service-v1-HpkeEncryptResponse)
  - [HpkeEncryptedData](#google-cmrt-sdk-crypto_service-v1-HpkeEncryptedData)
  - [HpkeParams](#google-cmrt-sdk-crypto_service-v1-HpkeParams)
  - [HpkeAead](#google-cmrt-sdk-crypto_service-v1-HpkeAead)
  - [HpkeKdf](#google-cmrt-sdk-crypto_service-v1-HpkeKdf)
  - [HpkeKem](#google-cmrt-sdk-crypto_service-v1-HpkeKem)
  - [SecretLength](#google-cmrt-sdk-crypto_service-v1-SecretLength)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_proto_crypto_service_v1_crypto_service-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/proto/crypto_service/v1/crypto_service.proto

<a name="google-cmrt-sdk-crypto_service-v1-AeadDecryptRequest"></a>

### AeadDecryptRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| encrypted_data | [AeadEncryptedData](#google-cmrt-sdk-crypto_service-v1-AeadEncryptedData) |  |  |
| secret | [bytes](#bytes) |  |  |
| shared_info | [string](#string) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-AeadDecryptResponse"></a>

### AeadDecryptResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| payload | [string](#string) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-AeadEncryptRequest"></a>

### AeadEncryptRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| payload | [string](#string) |  |  |
| secret | [bytes](#bytes) |  |  |
| shared_info | [string](#string) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-AeadEncryptResponse"></a>

### AeadEncryptResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| encrypted_data | [AeadEncryptedData](#google-cmrt-sdk-crypto_service-v1-AeadEncryptedData) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-AeadEncryptedData"></a>

### AeadEncryptedData

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| ciphertext | [bytes](#bytes) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeDecryptRequest"></a>

### HpkeDecryptRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| private_key | [google.cmrt.sdk.private_key_service.v1.PrivateKey](#google-cmrt-sdk-private_key_service-v1-PrivateKey) |  |  |
| encrypted_data | [HpkeEncryptedData](#google-cmrt-sdk-crypto_service-v1-HpkeEncryptedData) |  |  |
| shared_info | [string](#string) |  |  |
| hpke_params | [HpkeParams](#google-cmrt-sdk-crypto_service-v1-HpkeParams) |  |  |
| is_bidirectional | [bool](#bool) |  |  |
| exporter_context | [string](#string) |  |  |
| secret_length | [SecretLength](#google-cmrt-sdk-crypto_service-v1-SecretLength) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeDecryptResponse"></a>

### HpkeDecryptResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| payload | [string](#string) |  |  |
| secret | [bytes](#bytes) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeEncryptRequest"></a>

### HpkeEncryptRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| public_key | [google.cmrt.sdk.public_key_service.v1.PublicKey](#google-cmrt-sdk-public_key_service-v1-PublicKey) |  |  |
| payload | [string](#string) |  |  |
| shared_info | [string](#string) |  |  |
| hpke_params | [HpkeParams](#google-cmrt-sdk-crypto_service-v1-HpkeParams) |  |  |
| is_bidirectional | [bool](#bool) |  |  |
| exporter_context | [string](#string) |  |  |
| secret_length | [SecretLength](#google-cmrt-sdk-crypto_service-v1-SecretLength) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeEncryptResponse"></a>

### HpkeEncryptResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| encrypted_data | [HpkeEncryptedData](#google-cmrt-sdk-crypto_service-v1-HpkeEncryptedData) |  |  |
| secret | [bytes](#bytes) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeEncryptedData"></a>

### HpkeEncryptedData

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| ciphertext | [bytes](#bytes) |  |  |
| key_id | [string](#string) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeParams"></a>

### HpkeParams

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| kem | [HpkeKem](#google-cmrt-sdk-crypto_service-v1-HpkeKem) |  |  |
| kdf | [HpkeKdf](#google-cmrt-sdk-crypto_service-v1-HpkeKdf) |  |  |
| aead | [HpkeAead](#google-cmrt-sdk-crypto_service-v1-HpkeAead) |  |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeAead"></a>

### HpkeAead


| Name | Number | Description |
| ---- | ------ | ----------- |
| AEAD_UNSPECIFIED | 0 |  |
| AES_128_GCM | 1 |  |
| AES_256_GCM | 2 |  |
| CHACHA20_POLY1305 | 3 |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeKdf"></a>

### HpkeKdf


| Name | Number | Description |
| ---- | ------ | ----------- |
| KDF_UNSPECIFIED | 0 |  |
| HKDF_SHA256 | 1 |  |
<a name="google-cmrt-sdk-crypto_service-v1-HpkeKem"></a>

### HpkeKem


| Name | Number | Description |
| ---- | ------ | ----------- |
| KEM_UNSPECIFIED | 0 |  |
| DHKEM_X25519_HKDF_SHA256 | 1 |  |
<a name="google-cmrt-sdk-crypto_service-v1-SecretLength"></a>

### SecretLength


| Name | Number | Description |
| ---- | ------ | ----------- |
| SECRET_LENGTH_UNSPECIFIED | 0 |  |
| SECRET_LENGTH_16_BYTES | 1 |  |
| SECRET_LENGTH_32_BYTES | 2 |  |

## Scalar Value Types

| .proto Type | Notes | C++ | Java | Python | Go |
| ----------- | ----- | --- | ---- | ------ | -- |
| <a name="double" /> double |  | double | double | float | float64 |
| <a name="float" /> float |  | float | float | float | float32 |
| <a name="int32" /> int32 | Uses variable-length encoding. Inefficient for encoding negative numbers – if your field is likely to have negative values, use sint32 instead. | int32 | int | int | int32 |
| <a name="int64" /> int64 | Uses variable-length encoding. Inefficient for encoding negative numbers – if your field is likely to have negative values, use sint64 instead. | int64 | long | int/long | int64 |
| <a name="uint32" /> uint32 | Uses variable-length encoding. | uint32 | int | int/long | uint32 |
| <a name="uint64" /> uint64 | Uses variable-length encoding. | uint64 | long | int/long | uint64 |
| <a name="sint32" /> sint32 | Uses variable-length encoding. Signed int value. These more efficiently encode negative numbers than regular int32s. | int32 | int | int | int32 |
| <a name="sint64" /> sint64 | Uses variable-length encoding. Signed int value. These more efficiently encode negative numbers than regular int64s. | int64 | long | int/long | int64 |
| <a name="fixed32" /> fixed32 | Always four bytes. More efficient than uint32 if values are often greater than 2^28. | uint32 | int | int | uint32 |
| <a name="fixed64" /> fixed64 | Always eight bytes. More efficient than uint64 if values are often greater than 2^56. | uint64 | long | int/long | uint64 |
| <a name="sfixed32" /> sfixed32 | Always four bytes. | int32 | int | int | int32 |
| <a name="sfixed64" /> sfixed64 | Always eight bytes. | int64 | long | int/long | int64 |
| <a name="bool" /> bool |  | bool | boolean | boolean | bool |
| <a name="string" /> string | A string must always contain UTF-8 encoded or 7-bit ASCII text. | string | String | str/unicode | string |
| <a name="bytes" /> bytes | May contain any arbitrary sequence of bytes. | string | ByteString | str | []byte |
