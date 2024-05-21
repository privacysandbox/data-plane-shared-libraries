# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.proto](#src_public_cpio_proto_auto_scaling_service_v1_auto_scaling_service-proto)
  - [TryFinishInstanceTerminationRequest](#google-cmrt-sdk-auto_scaling_service-v1-TryFinishInstanceTerminationRequest)
  - [TryFinishInstanceTerminationResponse](#google-cmrt-sdk-auto_scaling_service-v1-TryFinishInstanceTerminationResponse)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_proto_auto_scaling_service_v1_auto_scaling_service-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/proto/auto_scaling_service/v1/auto_scaling_service.proto

<a name="google-cmrt-sdk-auto_scaling_service-v1-TryFinishInstanceTerminationRequest"></a>

### TryFinishInstanceTerminationRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| instance_resource_id | [string](#string) |  |  |
| lifecycle_hook_name | [string](#string) |  |  |
<a name="google-cmrt-sdk-auto_scaling_service-v1-TryFinishInstanceTerminationResponse"></a>

### TryFinishInstanceTerminationResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| termination_scheduled | [bool](#bool) |  |  |

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
