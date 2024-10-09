# Writing Test Cases

Each test case is defined by a set of files, all having the same filename prefix.

|             Suffix | Purpose                                    | Mandatory |
| -----------------: | ------------------------------------------ | --------- |
|    `.request.JSON` | Request                                    | required  |
|      `.reply.JSON` | Reply                                      | required  |
|   `.pre-filter.jq` | `jq` program to transform the request      | optional  |
|       `.filter.jq` | `jq` program to transform the reply        | optional  |
| `.filter.slurp.jq` | `jq` program to transform the reply stream | optional  |

## Request and reply

For a given gRPC functional test, a request and a reply will need to be specified using the format
`<test-name>.request.JSON` and `<test-name>.reply.JSON`. The reply file does not need to be in JSON
format, though the filename must still end `.reply.JSON`.

## jq filters

[jq](https://stedolan.github.io/jq) programs can be used to define pre- and/or post-filters.

### Pre-filter

Executed on the rpc request before it is sent to the server.

### Post-filter

Executed on the rpc response before `diff` is executed.

There are two ways a post-filter can be used: in normal mode and slurp mode. The difference is that
normal mode processes a single JSON object, and slurp mode processes a stream of JSON objects.
