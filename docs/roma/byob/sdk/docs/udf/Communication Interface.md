# Communication interface

Communication interfaces provide a way for the UDF to receive a request and send a response.

The communication to/from the UDF takes place over file descriptors (fd). The following sections
list the available streams, sockets and the expected communication protocol.

## Communication protocol

Protocol buffer (protobuf) messages are used for requests and responses. These are encoded using the
[protocol buffer wire format](https://protobuf.dev/programming-guides/encoding/). Functions provided
by the protobuf client library for your language can convert protobuf message objects to this binary
format. For example,
[in C++](https://protobuf.dev/getting-started/cpptutorial/#parsing-serialization),
`ParseFromIstream` and `SerializeToFileDescriptor`.

The input is read from standard input and the output is written to the fd provided as the first
argument to the UDF.

## Standard streams

### Standard input (stdin)

The serialized request should be read from stdin.

### Standard output (stdout)

Logs can be written to stdout. Currently, these logs are discarded. However, in future, logs may be
made available for debugging.

### Standard error (stderr)

Logs can be written to stderr. Currently, these logs are discarded. However, in future, logs may be
made available for debugging.

## Example UDF

Largely, a UDF's execution can be divided into four stages -

1. Setup

    During this stage, the UDF can initialize any variables, structures it may need. Note that this
    stage is optional.

1. Blocking read

    As the name suggests, during this stage the UDF is expected to wait for input to be written to
    the STDIN. It can do so using methods like `ParseFromFileDescriptor`.

1. Execution

    Leveraging the input, the UDF can execute the logic.

1. Write response

    Once the UDF response is constructed, it needs to be written to an fd. The fd will be provided
    as the first argument to the UDF. Functions provided by protobuf client library like
    `SerializeToFileDescriptor` can help with this.

1. Cleanup and exit

    Close the fd passed to the UDF, perform any cleanup tasks then exit.

A C++ example showcasing the stages. It can be extrapolated to other coding languages:

```cpp
int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 3) {
    LOG(ERROR) << "Not enough arguments!";
    return -1;
  }
  int response_fd;
  // The fist arg is the file descriptor for the UDF response message.
  CHECK(absl::SimpleAtoi(argv[1], &response_fd))
      << "Conversion of response file descriptor from string to int failed";

  EchoRequest request;
  // Read request from stdin.
  request.ParseFromIstream(&std::cin);

  // Create response.
  EchoResponse response;
  response.set_message(request.message());

  // Write response to provided response fd.
  response.SerializeToFileDescriptor(response_fd);
  return 0;
}
```

See more [example UDFs](/src/roma/byob/udf/).
