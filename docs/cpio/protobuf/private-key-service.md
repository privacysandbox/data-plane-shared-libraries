# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/proto/private_key_service/v1/private_key_service.proto](#src_public_cpio_proto_private_key_service_v1_private_key_service-proto)
  - [ListPrivateKeysRequest](#google-cmrt-sdk-private_key_service-v1-ListPrivateKeysRequest)
  - [ListPrivateKeysResponse](#google-cmrt-sdk-private_key_service-v1-ListPrivateKeysResponse)
  - [PrivateKey](#google-cmrt-sdk-private_key_service-v1-PrivateKey)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_proto_private_key_service_v1_private_key_service-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/proto/private_key_service/v1/private_key_service.proto

<a name="google-cmrt-sdk-private_key_service-v1-ListPrivateKeysRequest"></a>

### ListPrivateKeysRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key_ids | [string](#string) | repeated |  |
| max_age_seconds | [int64](#int64) |  |  |
<a name="google-cmrt-sdk-private_key_service-v1-ListPrivateKeysResponse"></a>

### ListPrivateKeysResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| private_keys | [PrivateKey](#google-cmrt-sdk-private_key_service-v1-PrivateKey) | repeated |  |
<a name="google-cmrt-sdk-private_key_service-v1-PrivateKey"></a>

### PrivateKey

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key_id | [string](#string) |  |  |
| public_key | [string](#string) |  |  |
| private_key | [string](#string) |  |  |
| expiration_time | [google.protobuf.Timestamp](#google-protobuf-Timestamp) |  |  |
| creation_time | [google.protobuf.Timestamp](#google-protobuf-Timestamp) |  |  |

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
