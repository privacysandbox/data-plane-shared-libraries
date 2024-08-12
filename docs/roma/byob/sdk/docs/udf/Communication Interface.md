# Communication interface

Communication interfaces provide a way for the UDF to receive a request and send a response.

The communication to/from the UDF takes place over streams and sockets. The following sections will
list the available streams, sockets and the expected communication protocol.

## Communication protocol

Protocol buffer (protobuf) messages are used for requests and responses. These are encoded using the
[protocol buffer wire format](https://protobuf.dev/programming-guides/encoding/). Functions provided
by the protobuf client library for your language can convert protobuf message objects to this binary
format. For example,
[in C++](https://protobuf.dev/getting-started/cpptutorial/#parsing-serialization),
`ParseFromIstream` and `SerializeToOstream`.

## Standard streams

### Standard input (stdin)

The serialized request should be read from stdin.

### Standard output (stdout)

The serialized response should be written to stdout. It is expected that only the response is
written to stdout.

### Standard error (stderr)

Logs can be written to stderr. Currently, these logs are discarded. However, in future, logs may be
made available for debugging.

## Example UDF

Largely, a binary's execution can be divided into four stages -

1. Setup

    During this stage, the binary can initialize any variables, structures it may need. Note that
    this stage is optional.

1. Blocking read

    As the name suggests, during this stage the binary is expected to wait for input to be written
    to the STDIN. It can do so using methods like `ParseFromFileDescriptor`.

1. Execution

    Leveraging the input, the binary can execute the logic.

1. Write response

    Once the binary is done executing, the response needs to be written to a socket. The socket fd
    will be provided as the first argument to the binary. Functions provided by protobuf client
    library like `SerializeToFileDescriptor` can help with this.

1. Cleanup and exit Close the socket to the binary, perform any cleanup tasks and exit.

A C++ example showcasing the stages. It can be extrapolated to other coding languages:

```cpp
int main(int argc, char* argv[]) {
  EchoRequest request;
  // Read request from stdin.
  request.ParseFromIstream(&std::cin);

  // Create response.
  EchoResponse response;
  response.set_message(request.message());

  // Write response to stdout.
  response.SerializeToOstream(&std::cout);
  return 0;
}
```

See more [example UDFs](/src/roma/gvisor/udf/).
