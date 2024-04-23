# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/proto/instance_service/v1/instance_service.proto](#src_public_cpio_proto_instance_service_v1_instance_service-proto)
  - [GetCurrentInstanceResourceNameRequest](#google-cmrt-sdk-instance_service-v1-GetCurrentInstanceResourceNameRequest)
  - [GetCurrentInstanceResourceNameResponse](#google-cmrt-sdk-instance_service-v1-GetCurrentInstanceResourceNameResponse)
  - [GetInstanceDetailsByResourceNameRequest](#google-cmrt-sdk-instance_service-v1-GetInstanceDetailsByResourceNameRequest)
  - [GetInstanceDetailsByResourceNameResponse](#google-cmrt-sdk-instance_service-v1-GetInstanceDetailsByResourceNameResponse)
  - [GetTagsByResourceNameRequest](#google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameRequest)
  - [GetTagsByResourceNameResponse](#google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameResponse)
  - [GetTagsByResourceNameResponse.TagsEntry](#google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameResponse-TagsEntry)
  - [InstanceDetails](#google-cmrt-sdk-instance_service-v1-InstanceDetails)
  - [InstanceDetails.LabelsEntry](#google-cmrt-sdk-instance_service-v1-InstanceDetails-LabelsEntry)
  - [InstanceNetwork](#google-cmrt-sdk-instance_service-v1-InstanceNetwork)
  - [ListInstanceDetailsByEnvironmentRequest](#google-cmrt-sdk-instance_service-v1-ListInstanceDetailsByEnvironmentRequest)
  - [ListInstanceDetailsByEnvironmentResponse](#google-cmrt-sdk-instance_service-v1-ListInstanceDetailsByEnvironmentResponse)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_proto_instance_service_v1_instance_service-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/proto/instance_service/v1/instance_service.proto

<a name="google-cmrt-sdk-instance_service-v1-GetCurrentInstanceResourceNameRequest"></a>

### GetCurrentInstanceResourceNameRequest

<a name="google-cmrt-sdk-instance_service-v1-GetCurrentInstanceResourceNameResponse"></a>

### GetCurrentInstanceResourceNameResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| instance_resource_name | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-GetInstanceDetailsByResourceNameRequest"></a>

### GetInstanceDetailsByResourceNameRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| instance_resource_name | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-GetInstanceDetailsByResourceNameResponse"></a>

### GetInstanceDetailsByResourceNameResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| instance_details | [InstanceDetails](#google-cmrt-sdk-instance_service-v1-InstanceDetails) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameRequest"></a>

### GetTagsByResourceNameRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| resource_name | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameResponse"></a>

### GetTagsByResourceNameResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| tags | [GetTagsByResourceNameResponse.TagsEntry](#google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameResponse-TagsEntry) | repeated |  |
<a name="google-cmrt-sdk-instance_service-v1-GetTagsByResourceNameResponse-TagsEntry"></a>

### GetTagsByResourceNameResponse.TagsEntry

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key | [string](#string) |  |  |
| value | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-InstanceDetails"></a>

### InstanceDetails

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| instance_id | [string](#string) |  |  |
| networks | [InstanceNetwork](#google-cmrt-sdk-instance_service-v1-InstanceNetwork) | repeated |  |
| labels | [InstanceDetails.LabelsEntry](#google-cmrt-sdk-instance_service-v1-InstanceDetails-LabelsEntry) | repeated |  |
<a name="google-cmrt-sdk-instance_service-v1-InstanceDetails-LabelsEntry"></a>

### InstanceDetails.LabelsEntry

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key | [string](#string) |  |  |
| value | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-InstanceNetwork"></a>

### InstanceNetwork

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| public_ipv4_address | [string](#string) |  |  |
| private_ipv4_address | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-ListInstanceDetailsByEnvironmentRequest"></a>

### ListInstanceDetailsByEnvironmentRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| environment | [string](#string) |  |  |
| project_id | [string](#string) |  |  |
| page_token | [string](#string) |  |  |
<a name="google-cmrt-sdk-instance_service-v1-ListInstanceDetailsByEnvironmentResponse"></a>

### ListInstanceDetailsByEnvironmentResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
| instance_details | [InstanceDetails](#google-cmrt-sdk-instance_service-v1-InstanceDetails) | repeated |  |
| page_token | [string](#string) |  |  |

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
