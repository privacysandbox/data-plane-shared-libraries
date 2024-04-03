# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.proto](#src_public_cpio_proto_blob_storage_service_v1_blob_storage_service-proto)
  - [Blob](#google-cmrt-sdk-blob_storage_service-v1-Blob)
  - [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata)
  - [ByteRange](#google-cmrt-sdk-blob_storage_service-v1-ByteRange)
  - [DeleteBlobRequest](#google-cmrt-sdk-blob_storage_service-v1-DeleteBlobRequest)
  - [DeleteBlobResponse](#google-cmrt-sdk-blob_storage_service-v1-DeleteBlobResponse)
  - [DeleteBucketRequest](#google-cmrt-sdk-blob_storage_service-v1-DeleteBucketRequest)
  - [DeleteBucketResponse](#google-cmrt-sdk-blob_storage_service-v1-DeleteBucketResponse)
  - [GetBlobRequest](#google-cmrt-sdk-blob_storage_service-v1-GetBlobRequest)
  - [GetBlobResponse](#google-cmrt-sdk-blob_storage_service-v1-GetBlobResponse)
  - [GetBlobStreamRequest](#google-cmrt-sdk-blob_storage_service-v1-GetBlobStreamRequest)
  - [GetBlobStreamResponse](#google-cmrt-sdk-blob_storage_service-v1-GetBlobStreamResponse)
  - [ListBlobsMetadataRequest](#google-cmrt-sdk-blob_storage_service-v1-ListBlobsMetadataRequest)
  - [ListBlobsMetadataResponse](#google-cmrt-sdk-blob_storage_service-v1-ListBlobsMetadataResponse)
  - [PutBlobRequest](#google-cmrt-sdk-blob_storage_service-v1-PutBlobRequest)
  - [PutBlobResponse](#google-cmrt-sdk-blob_storage_service-v1-PutBlobResponse)
  - [PutBlobStreamRequest](#google-cmrt-sdk-blob_storage_service-v1-PutBlobStreamRequest)
  - [PutBlobStreamResponse](#google-cmrt-sdk-blob_storage_service-v1-PutBlobStreamResponse)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_proto_blob_storage_service_v1_blob_storage_service-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/proto/blob_storage_service/v1/blob_storage_service.proto

<a name="google-cmrt-sdk-blob_storage_service-v1-Blob"></a>

### Blob

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| metadata | [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata) |  |  |
| data | [bytes](#bytes) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-BlobMetadata"></a>

### BlobMetadata

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| bucket_name | [string](#string) |  |  |
| blob_name | [string](#string) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-ByteRange"></a>

### ByteRange

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| begin_byte_index | [uint64](#uint64) |  |  |
| end_byte_index | [uint64](#uint64) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-DeleteBlobRequest"></a>

### DeleteBlobRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| blob_metadata | [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-DeleteBlobResponse"></a>

### DeleteBlobResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-DeleteBucketRequest"></a>

### DeleteBucketRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| bucket_name | [string](#string) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-DeleteBucketResponse"></a>

### DeleteBucketResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-GetBlobRequest"></a>

### GetBlobRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| blob_metadata | [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata) |  |  |
| byte_range | [ByteRange](#google-cmrt-sdk-blob_storage_service-v1-ByteRange) | optional |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-GetBlobResponse"></a>

### GetBlobResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| blob | [Blob](#google-cmrt-sdk-blob_storage_service-v1-Blob) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-GetBlobStreamRequest"></a>

### GetBlobStreamRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| blob_metadata | [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata) |  |  |
| byte_range | [ByteRange](#google-cmrt-sdk-blob_storage_service-v1-ByteRange) | optional |  |
| max_bytes_per_response | [uint64](#uint64) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-GetBlobStreamResponse"></a>

### GetBlobStreamResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| blob_portion | [Blob](#google-cmrt-sdk-blob_storage_service-v1-Blob) |  |  |
| byte_range | [ByteRange](#google-cmrt-sdk-blob_storage_service-v1-ByteRange) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-ListBlobsMetadataRequest"></a>

### ListBlobsMetadataRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| blob_metadata | [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata) |  |  |
| page_token | [string](#string) | optional |  |
| max_page_size | [uint64](#uint64) | optional |  |
| exclude_directories | [bool](#bool) | optional |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-ListBlobsMetadataResponse"></a>

### ListBlobsMetadataResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| blob_metadatas | [BlobMetadata](#google-cmrt-sdk-blob_storage_service-v1-BlobMetadata) | repeated |  |
| next_page_token | [string](#string) | optional |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-PutBlobRequest"></a>

### PutBlobRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| blob | [Blob](#google-cmrt-sdk-blob_storage_service-v1-Blob) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-PutBlobResponse"></a>

### PutBlobResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-PutBlobStreamRequest"></a>

### PutBlobStreamRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| blob_portion | [Blob](#google-cmrt-sdk-blob_storage_service-v1-Blob) |  |  |
| stream_keepalive_duration | [google.protobuf.Duration](#google-protobuf-Duration) | optional |  |
<a name="google-cmrt-sdk-blob_storage_service-v1-PutBlobStreamResponse"></a>

### PutBlobStreamResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |

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
