# Roma Host Api

Roma provides proto-based C++ and JS generated functions to allow for greater structure in the
declaration and usage of C++ hook functions within JS.

## Creating a Host Api Proto

With this new Host Api functionality, Roma integrators can define a `service`, annotated with
`privacysandbox.apis.roma.app_api.v1.roma_svc_annotation`. These annotations include
`cpp_namespace`, `roma_app_name`, and `cpp_host_process_callback_includes`, which are used to
customize the generated C++ code.

### Service Properties

-   `cpp_namespace` - Namespace of generated C++ code
-   `roma_app_name` - Name for Service in JS
-   `cpp_host_process_callback_includes` - List of include paths to C++ hook function declarations
-   `description` - (optional) Description of Host Api Service

### Example

```proto
service TestHostService {
  option (privacysandbox.apis.roma.app_api.v1.roma_svc_annotation) = {
    cpp_namespace: "privacysandbox::host_server"
    roma_app_name: "TestHostService"
    cpp_host_process_callback_includes: "src/roma/native_function_grpc_server/proto/test_service_native_functions.h"
  };
}
```

Each service is annotated with RPC methods corresponding to each C++ hook function they want to make
available in JS. Each rpc is similarly annotated with `cpp_host_process_callback`, which is also
used to customize the generated C++ code. See
[options.proto](/apis/privacysandbox/apis/roma/app_api/v1/options.proto) for more information on
annotations.

### RPC Properties

-   `cpp_host_process_callback` - Fully qualified name of C++ hook function invoked for this RPC.
    This hook function would accept the metadata attached to the C++ hook function, `metadata` and a
    const& to the `request` message, and return a `response` message and an `absl::Status`
    indicating the status of the C++ hook function. See [metadata.md](metadata.md) for more details
    on `metadata`.

See [test_host_service.proto](/src/roma/native_function_grpc_server/proto/test_host_service.proto)
for an example of a service defined with an RPC, `NativeMethod`. Notice that `TestHostService`'s
`cpp_host_process_callback_includes` contains
[src/roma/native_function_grpc_server/proto/test_service_native_functions.h](/src/roma/native_function_grpc_server/proto/test_service_native_functions.h),
and test_service_native_functions.h has the fully qualified name to the C++ hook function that
should be invoked for NativeMethod in JS, `::privacy_sandbox::test_host_server::HandleNativeMethod`,
which is found in NativeMethod's `cpp_host_process_callback` property. The corresponding BUILD file,
found [here](/src/roma/native_function_grpc_server/proto/BUILD) gives an example of how to use the
new build rules to build the Host Api for your proto.

Declaring a proto with C++ hook functions in this fashion will generate a C++ function:

```cpp
void cpp_namespace::RegisterHostApi(Config config);
```

Which accepts a `Config` object and registers all RPC methods from that service to make them
available from JS. In JS, each RPC method will be able to be invoked in the host process using
`roma_app_name.rpcName` (`TestHostService.NativeMethod` for example).

See `EncodeDecodeProtobufWithNativeCallback` in
[test_service_roma_api_test.cc](/src/roma/native_function_grpc_server/proto/test_service_roma_api_test.cc)
for an example that uses `RegisterHostApi` to register the host api for `test_host_service.proto`
and then invokes the C++ hook function in the host process using `roma_app_name.rpcName`.

## Performance

| Benchmark                                     | Time     | CPU      | Iterations |
| --------------------------------------------- | -------- | -------- | ---------- |
| (1) BM_NonDeclarativeApiNativeFunctionHandler | 0.654 ms | 0.023 ms | 10000      |
| (2) BM_DeclarativeApiNativeFunctionHandler    | 0.673 ms | 0.022 ms | 10000      |
| (3) BM_DeclarativeApiGrpcServer               | 0.843 ms | 0.021 ms | 10000      |

This benchmark measures the time it takes to execute a simple UDF that invokes a C++ hook function:

1.  Using the prior `config.RegisterFunctionBinding()` approach
2.  Using the proto based Host Api approach
3.  Using the proto based Host Api approach with a gRPC server to communicate with the host process

Benchmark code can be found [here](/src/roma/benchmark/host_api_grpc_benchmark.cc).
