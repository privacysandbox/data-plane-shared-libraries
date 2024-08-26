# Communication interface

Communication interfaces provide a way for the UDF to receive a request and send a response.

The communication to/from the UDF takes place over a file descriptor (fd). The following sections
list the available streams, sockets and the expected communication protocol.

## Communication protocol

Protocol buffer (protobuf) messages are used for requests and responses. The proto messages are
packed and unpacked from [Any](https://protobuf.dev/programming-guides/proto3/#any). Functions
provided by the protobuf client library for your language can convert this Any protobuf to
[size-delimited binary format](https://protobuf.dev/programming-guides/encoding/) for passing
over-the-file-descriptor . For example, in
[C++](https://protobuf.dev/getting-started/cpptutorial/#parsing-serialization),
`ParseDelimitedFromZeroCopyStream` and `SerializeDelimitedToFileDescriptor`. The section below
offers an illustrative example.

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
    file descriptor. The fd will be provided as the first argument to the UDF.

    The input will be written as a `Any`-packed, size-delimited proto. After reading the
    size-delimited `Any` message object, the input can be unpacked from `Any` using `UnpackFrom`.
    For parsing the `Any` message from the size-delimited binary format, methods like
    `ParseDelimitedFromZeroCopyStream` can be used.

1. Execution

    Leveraging the input, the UDF can execute the logic.

1. Write response

    Once the UDF response is constructed, it needs to be written to an fd. The fd will be provided
    as the first argument to the UDF. The output should be written as `Any`-packed, size-delimited
    proto.

    The response message object can be packed to `Any` using `PackFrom`. Then the `Any` message
    object can be written size-delimited to the fd. For serializing the `Any` message to the
    size-delimited binary format, methods like `SerializeDelimitedToFileDescriptor` can be used.

1. Cleanup and exit

    Close the fd passed to the UDF, perform any cleanup tasks then exit.

A C++ example showcasing the stages. It can be extrapolated to other coding languages:

```cpp
int main(int argc, char* argv[]) {
  absl::InitializeLog();
  if (argc < 2) {
    LOG(ERROR) << "Not enough arguments!";
    return -1;
  }
  int32_t fd;
  CHECK(absl::SimpleAtoi(argv[1], &fd))
      << "Conversion of write file descriptor string to int failed";
  EchoRequest request;
  // Any initialization work can be done before this point.
  // The following line will result in a blocking read being performed by the
  // binary i.e. waiting for input before execution.
  // The EchoRequest proto is defined by the Trusted Server team. The UDF reads
  // request from the provided file descriptor.
  {
    ::google::protobuf::Any any;
    ::google::protobuf::io::FileInputStream input(fd);
    ::google::protobuf::util::ParseDelimitedFromZeroCopyStream(&any, &input,
                                                               nullptr);
    any.UnpackTo(&request);
  }

  EchoResponse response;
  response.set_message(request.message());

  // Once the UDF is done executing, it should write the response (EchoResponse
  // in this case) to the provided file descriptor.
  ::google::protobuf::Any any;
  any.PackFrom(response);
  ::google::protobuf::util::SerializeDelimitedToFileDescriptor(any, fd);
  return 0;
}
```

See more [example UDFs](/src/roma/byob/example/).
