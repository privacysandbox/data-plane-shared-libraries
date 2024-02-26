# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/validator/proto/validator_config.proto](#src_public_cpio_validator_proto_validator_config-proto)
  - [DnsConfig](#google-scp-cpio-validator-proto-DnsConfig)
  - [EnqueueMessageConfig](#google-scp-cpio-validator-proto-EnqueueMessageConfig)
  - [FetchPrivateKeyConfig](#google-scp-cpio-validator-proto-FetchPrivateKeyConfig)
  - [GetBlobConfig](#google-scp-cpio-validator-proto-GetBlobConfig)
  - [GetCurrentInstanceResourceNameConfig](#google-scp-cpio-validator-proto-GetCurrentInstanceResourceNameConfig)
  - [GetParameterConfig](#google-scp-cpio-validator-proto-GetParameterConfig)
  - [GetTagsByResourceNameConfig](#google-scp-cpio-validator-proto-GetTagsByResourceNameConfig)
  - [GetTopMessageConfig](#google-scp-cpio-validator-proto-GetTopMessageConfig)
  - [HttpConfig](#google-scp-cpio-validator-proto-HttpConfig)
  - [HttpConfig.RequestHeadersEntry](#google-scp-cpio-validator-proto-HttpConfig-RequestHeadersEntry)
  - [ListBlobsMetadataConfig](#google-scp-cpio-validator-proto-ListBlobsMetadataConfig)
  - [TestCase](#google-scp-cpio-validator-proto-TestCase)
  - [ValidatorConfig](#google-scp-cpio-validator-proto-ValidatorConfig)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_validator_proto_validator_config-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/validator/proto/validator_config.proto

<a name="google-scp-cpio-validator-proto-DnsConfig"></a>

### DnsConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| host | [string](#string) |  |  |
| port | [uint32](#uint32) |  |  |
<a name="google-scp-cpio-validator-proto-EnqueueMessageConfig"></a>

### EnqueueMessageConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| message_body | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-FetchPrivateKeyConfig"></a>

### FetchPrivateKeyConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key_id | [string](#string) |  |  |
| private_key_vending_service_endpoint | [string](#string) |  |  |
| service_region | [string](#string) |  |  |
| account_identity | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-GetBlobConfig"></a>

### GetBlobConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| bucket_name | [string](#string) |  |  |
| blob_name | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-GetCurrentInstanceResourceNameConfig"></a>

### GetCurrentInstanceResourceNameConfig

<a name="google-scp-cpio-validator-proto-GetParameterConfig"></a>

### GetParameterConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| parameter_name | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-GetTagsByResourceNameConfig"></a>

### GetTagsByResourceNameConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| resource_name | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-GetTopMessageConfig"></a>

### GetTopMessageConfig

<a name="google-scp-cpio-validator-proto-HttpConfig"></a>

### HttpConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| request_method | [string](#string) |  |  |
| request_url | [string](#string) |  |  |
| request_headers | [HttpConfig.RequestHeadersEntry](#google-scp-cpio-validator-proto-HttpConfig-RequestHeadersEntry) | repeated |  |
<a name="google-scp-cpio-validator-proto-HttpConfig-RequestHeadersEntry"></a>

### HttpConfig.RequestHeadersEntry

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key | [string](#string) |  |  |
| value | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-ListBlobsMetadataConfig"></a>

### ListBlobsMetadataConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| bucket_name | [string](#string) |  |  |
<a name="google-scp-cpio-validator-proto-TestCase"></a>

### TestCase

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| name | [string](#string) |  |  |
| dns_config | [DnsConfig](#google-scp-cpio-validator-proto-DnsConfig) |  |  |
| http_config | [HttpConfig](#google-scp-cpio-validator-proto-HttpConfig) |  |  |
| get_tags_by_resource_name_config | [GetTagsByResourceNameConfig](#google-scp-cpio-validator-proto-GetTagsByResourceNameConfig) |  |  |
| get_current_instance_resource_name_config | [GetCurrentInstanceResourceNameConfig](#google-scp-cpio-validator-proto-GetCurrentInstanceResourceNameConfig) |  |  |
| get_blob_config | [GetBlobConfig](#google-scp-cpio-validator-proto-GetBlobConfig) |  |  |
| list_blobs_metadata_config | [ListBlobsMetadataConfig](#google-scp-cpio-validator-proto-ListBlobsMetadataConfig) |  |  |
| get_parameter_config | [GetParameterConfig](#google-scp-cpio-validator-proto-GetParameterConfig) |  |  |
| enqueue_message_config | [EnqueueMessageConfig](#google-scp-cpio-validator-proto-EnqueueMessageConfig) |  |  |
| get_top_message_config | [GetTopMessageConfig](#google-scp-cpio-validator-proto-GetTopMessageConfig) |  |  |
| fetch_private_key_config | [FetchPrivateKeyConfig](#google-scp-cpio-validator-proto-FetchPrivateKeyConfig) |  |  |
<a name="google-scp-cpio-validator-proto-ValidatorConfig"></a>

### ValidatorConfig

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| test_cases | [TestCase](#google-scp-cpio-validator-proto-TestCase) | repeated |  |

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
