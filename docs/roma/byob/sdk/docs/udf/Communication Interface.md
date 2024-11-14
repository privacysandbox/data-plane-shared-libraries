# Communication interface

Communication interfaces provide a way for the UDF to receive a request and send a response.

The communication to/from the UDF takes place over a file descriptor (fd). The following sections
list the available streams, sockets and the expected wire format.

## Standard streams

### Standard input (stdin)

Available for use.

### Standard output (stdout)

Logs can be written to stdout. Currently, these logs are discarded. However, in future, logs may be
made available for debugging.

### Standard error (stderr)

Logs can be written to stderr. Currently, these logs are discarded. However, in future, logs may be
made available for debugging.

## Command-line flags

The file descriptor (fd) is the only argument specified to the UDF. It is a positive integer value,
and is provided as a positional argument.

## Wire format

The request/response to the UDF will be will be passed over the fd as
[size-delimited](https://protobuf.dev/programming-guides/encoding/) protobuf messages.

### Reading messages

To read a message from an fd using the protobuf C++ library &mdash; equivalents are available in
other languages:

-   Construct an
    [io::FileInputStream](https://protobuf.dev/reference/cpp/api-docs/google.protobuf.io.zero_copy_stream_impl/)
    for the fd.
-   Create a message of the expected type.
-   Use
    [ParseDelimitedFromZeroCopyStream()](https://github.com/protocolbuffers/protobuf/blob/182699f8fde413e3fdc770ecf3808fe2f2fe01c3/src/google/protobuf/util/delimited_message_util.h#L49-L62)
    to populate the message.

### Writing messages

To write a message to an fd:

-   Write the message to the fd in delimited binary format by using the
    [SerializeDelimitedToFileDescriptor](https://github.com/protocolbuffers/protobuf/blob/182699f8fde413e3fdc770ecf3808fe2f2fe01c3/src/google/protobuf/util/delimited_message_util.h#L27-L44)
    method.

## Example UDF

Largely, a UDF's execution can be divided into following stages -

1. Setup

    During this stage, the UDF can initialize any variables, structures it may need. Note that this
    stage is optional.

1. Blocking read

    As the name suggests, during this stage the UDF is expected to wait for input to be written to
    the file descriptor.

1. Execution

    Leveraging the input, the UDF can execute the logic.

1. Write response

    Once the UDF response is constructed, it needs to be written to the fd. The output should be
    written as a size-delimited proto.

1. Cleanup and exit

    Perform any cleanup tasks then exit.

A C++ example showcasing the stages. It can be extrapolated to other coding languages:

```cpp
#include <iostream>

#include "google/protobuf/util/delimited_message_util.h"
#include "src/roma/byob/example/example.pb.h"

using ::privacy_sandbox::server_common::byob::example::EchoRequest;
using ::privacy_sandbox::server_common::byob::example::EchoResponse;

EchoRequest ReadRequestFromFd(int fd) {
  EchoRequest req;
  google::protobuf::io::FileInputStream stream(fd);
  google::protobuf::util::ParseDelimitedFromZeroCopyStream(&req, &stream,
                                                           nullptr);
  return req;
}

void WriteResponseToFd(int fd, EchoResponse resp) {
  google::protobuf::util::SerializeDelimitedToFileDescriptor(resp, fd);
}

int main(int argc, char* argv[]) {
  if (argc != 2) {
    std::cerr << "Expecting exactly one argument";
    return -1;
  }
  int fd = std::stoi(argv[1]);

  // Any initialization work can be done before this point.
  // The following line will result in a blocking read being performed by the
  // binary i.e. waiting for input before execution.
  // The EchoRequest proto is defined by the Trusted Server team. The UDF reads
  // request from the provided file descriptor.
  EchoRequest request = ReadRequestFromFd(fd);

  EchoResponse response;
  response.set_message(request.message());

  // Once the UDF is done executing, it should write the response (EchoResponse
  // in this case) to the provided file descriptor.
  WriteResponseToFd(fd, std::move(response));
  return 0;
}
```

See more
[example UDFs](https://github.com/privacysandbox/data-plane-shared-libraries/tree/e5d685e2d07b4535b650e4f44f8473e187408fc6/src/roma/byob/example).
