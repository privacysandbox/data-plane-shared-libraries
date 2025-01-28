# Overview

**Note:** _This is a draft API proposal, and not necessarily the final version. It has not been
decided if and when this API will be integrated with the TEE servers._

Parc is an API definition intended to allow trusted servers running in Trusted Execution
Environments
[TEEs](https://github.com/privacysandbox/protected-auction-services-docs/blob/main/public_cloud_tees.md)
to receive data from untrusted data sources. In a cloud environment, a server implementing the Parc
API (a "Parc server") would sit between trusted servers running in TEEs and cloud services to
provide arbitrary data needed by the trusted servers.

The Parc service interface abstracts away the specific cloud implemented services which provides
blobs, parameters, or other data so that trusted servers need only interact with the Parc server to
retrieve data only available at runtime. This has the added benefit of minimizing the threat surface
for the trusted servers by minimizing the infrastructure-specific code package into the trusted
executable.

# Parc API

The Parc API definitions can be found in
[`apis/privacysandbox/apis/parc/`](/apis/privacysandbox/apis/parc/). The API is encapsulated in a
service definition found in
[`parc_service.proto`](/apis/privacysandbox/apis/parc/v0/parc_service.proto). The service defines
four rpcs:

-   **GetBlob:** A response-streaming rpc which provides arbitrary data from a requested source.
-   **ListBlobsMetadata:** a unary rpc which lists metadata for blobs in a data bucket.
-   **GetBlobMetadata:** a unary rpc which retrieves detailed metadata for a single blob.
-   **GetParameter:** a unary rpc which retrieves a value under a given key from parameter storage.

## Interface RPCs

### GetBlob

Protocol buffers spec:
[`apis/privacysandbox/apis/parc/{version}/blob.proto`](/apis/privacysandbox/apis/parc/v0/blob.proto)

This rpc requests a given blob specified by a `GetBlobRequest`. Example request:

```protobuf
{
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob1"
  }
  byte_range {
    begin: 100
    end: 150
  }
}
```

Buckets are locations in which blobs can be found and referred to by name. An optional byte range
can be provided which will request the bytes specified in the closed interval `[begin,end]`. If no
byte range is specified, the entire blob is retrieved.

A Parc server would respond with a sequential streamed series of `GetBlobResponse`. The number of
stream responses depends on the max sent message chunk size from the server.

Example response:

```protobuf
{
  data: "some arbitrary data"
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob1"
  }
  content_encoding: CONTENT_ENCODING_PLAINTEXT
}
```

Note that `blob_metadata` and `content_encoding` are provided as part of the first-streamed response
only, and are provided for validation purposes.

### ListBlobsMetadata

Protocol buffers spec:
[`apis/privacysandbox/apis/parc/{version}/blob.proto`](/apis/privacysandbox/apis/parc/v0/blob.proto)

This rpc requests metadata for blobs in a given blob container ("bucket"). The request is a
`ListBlobsMetadataRequest`

Example request:

```protobuf
{
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob"
  }
  page_token: "3"
  max_page_size: 500
  exclude_directories: true
}
```

This will request the blob metadata for all blobs in "bucket1". The `blob_name` field is used as a
prefix to match names starting with the provided string. The `page_token` field and `max_page_size`
fields are used to paginate through blob containers. The `exlude_directories` field specifies
whether or not to return blob containers.

Example response:

```protobuf
{
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob1"
  }
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob2"
  }
  next_page_token: "4"
}
```

The response contains a repeated series of blob metadata structs which match the blobs whose prefix
matches the `blob_name` field in the request. A token for the next page is also returned.

### GetBlobMetadata

Protocol buffers spec:
[`apis/privacysandbox/apis/parc/{version}/blob.proto`](/apis/privacysandbox/apis/parc/v0/blob.proto)

This rpc requests a single blob's metadata. This provides more detailed data for a single blob.

Example request:

```protobuf
{
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob1"
  }
}
```

Example response:

```protobuf
{
  blob_metadata {
    bucket_name: "bucket1"
    blob_name: "blob1"
    blob_size: 1024
    hashes {
      hash: "E9CBFBE5"
      type: HASH_TYPE_CRC32C
    }
  }
}
```

The `blob_size` fields returns the number of bytes in the full blob. The `hashes` field is repeated
an contains the hash values of the full blob along with the hash type.

### GetParameter

Protocol buffers spec:
[`apis/privacysandbox/apis/parc/{version}/parameter.proto`](/apis/privacysandbox/apis/parc/v0/parameter.proto)

This rpc requests a value from a parameter storage service. This assumes an interface where a given
key is mapped to a single value.

Example request:

```protobuf
{
  parameter_name: "some_parameter_name"
}
```

Example response:

```protobuf
{
  parameter_value: "some_parameter_value"
}
```

This rpc is broadly intended to request specific configuration data for the given service without
needing to bake in the parameter into the trusted executable.
