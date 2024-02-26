# Privacy Sandbox CPIO Protobuf Documentation
<a name="top"></a>

## Table of Contents

- [src/public/cpio/proto/metric_service/v1/metric_service.proto](#src_public_cpio_proto_metric_service_v1_metric_service-proto)
  - [Metric](#google-cmrt-sdk-metric_service-v1-Metric)
  - [Metric.LabelsEntry](#google-cmrt-sdk-metric_service-v1-Metric-LabelsEntry)
  - [PutMetricsRequest](#google-cmrt-sdk-metric_service-v1-PutMetricsRequest)
  - [PutMetricsResponse](#google-cmrt-sdk-metric_service-v1-PutMetricsResponse)
  - [MetricUnit](#google-cmrt-sdk-metric_service-v1-MetricUnit)
- [Scalar Value Types](#scalar-value-types)

<a name="src_public_cpio_proto_metric_service_v1_metric_service-proto"></a>
<p align="right"><a href="#top">Top</a></p>

## src/public/cpio/proto/metric_service/v1/metric_service.proto

<a name="google-cmrt-sdk-metric_service-v1-Metric"></a>

### Metric

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| name | [string](#string) |  |  |
| value | [string](#string) |  |  |
| unit | [MetricUnit](#google-cmrt-sdk-metric_service-v1-MetricUnit) |  |  |
| labels | [Metric.LabelsEntry](#google-cmrt-sdk-metric_service-v1-Metric-LabelsEntry) | repeated |  |
| timestamp | [google.protobuf.Timestamp](#google-protobuf-Timestamp) |  |  |
<a name="google-cmrt-sdk-metric_service-v1-Metric-LabelsEntry"></a>

### Metric.LabelsEntry

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| key | [string](#string) |  |  |
| value | [string](#string) |  |  |
<a name="google-cmrt-sdk-metric_service-v1-PutMetricsRequest"></a>

### PutMetricsRequest

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| metric_namespace | [string](#string) |  |  |
| metrics | [Metric](#google-cmrt-sdk-metric_service-v1-Metric) | repeated |  |
<a name="google-cmrt-sdk-metric_service-v1-PutMetricsResponse"></a>

### PutMetricsResponse

| Field | Type | Label | Description |
| ----- | ---- | ----- | ----------- |
| result | [google.scp.core.common.proto.ExecutionResult](#google-scp-core-common-proto-ExecutionResult) |  |  |
<a name="google-cmrt-sdk-metric_service-v1-MetricUnit"></a>

### MetricUnit


| Name | Number | Description |
| ---- | ------ | ----------- |
| METRIC_UNIT_UNSPECIFIED | 0 |  |
| METRIC_UNIT_SECONDS | 1 |  |
| METRIC_UNIT_MICROSECONDS | 2 |  |
| METRIC_UNIT_MILLISECONDS | 3 |  |
| METRIC_UNIT_BITS | 4 |  |
| METRIC_UNIT_KILOBITS | 5 |  |
| METRIC_UNIT_MEGABITS | 6 |  |
| METRIC_UNIT_GIGABITS | 7 |  |
| METRIC_UNIT_TERABITS | 8 |  |
| METRIC_UNIT_BYTES | 9 |  |
| METRIC_UNIT_KILOBYTES | 10 |  |
| METRIC_UNIT_MEGABYTES | 11 |  |
| METRIC_UNIT_GIGABYTES | 12 |  |
| METRIC_UNIT_TERABYTES | 13 |  |
| METRIC_UNIT_COUNT | 14 |  |
| METRIC_UNIT_PERCENT | 15 |  |
| METRIC_UNIT_BITS_PER_SECOND | 16 |  |
| METRIC_UNIT_KILOBITS_PER_SECOND | 17 |  |
| METRIC_UNIT_MEGABITS_PER_SECOND | 18 |  |
| METRIC_UNIT_GIGABITS_PER_SECOND | 19 |  |
| METRIC_UNIT_TERABITS_PER_SECOND | 20 |  |
| METRIC_UNIT_BYTES_PER_SECOND | 21 |  |
| METRIC_UNIT_KILOBYTES_PER_SECOND | 22 |  |
| METRIC_UNIT_MEGABYTES_PER_SECOND | 23 |  |
| METRIC_UNIT_GIGABYTES_PER_SECOND | 24 |  |
| METRIC_UNIT_TERABYTES_PER_SECOND | 25 |  |
| METRIC_UNIT_COUNT_PER_SECOND | 26 |  |

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
