// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package main

import (
	"bufio"
	pb "github.com/privacy-sandbox/data-plane-shared/apis/roma/binary/example"
	protodelim "google.golang.org/protobuf/encoding/protodelim"
	"io"
	"log"
	"os"
	"strconv"
)

func ReadRequestFromFd(reader protodelim.Reader) pb.EchoRequest {
	request := pb.EchoRequest{}
	err := protodelim.UnmarshalFrom(reader, &request)
	if err == io.EOF {
		os.Exit(-1)
	} else if err != nil {
		log.Fatal("Failed to read proto: ", err)
	}
	return request
}

func WriteResponseToFd(writer io.Writer, response pb.EchoResponse) {
	if _, err := protodelim.MarshalTo(writer, &response); err != nil {
		log.Fatal("Failed to write response proto: %v", err)
	}
}

func main() {
	if len(os.Args) != 2 {
		log.Fatal("Expecting exactly one argument")
	}
	fd, err := strconv.Atoi(os.Args[1])
	if err != nil {
		log.Fatal("Missing file descriptor: ", err)
	}
	file := os.NewFile(uintptr(fd), "socket")
	defer file.Close()
	// Any initialization work can be done before this point.
	// The following line will result in a blocking read being performed by the
	// binary i.e. waiting for input before execution.
	// The EchoRequest proto is defined by the Trusted Server team.
	// The UDF reads request from the provided fd.
	request := ReadRequestFromFd(bufio.NewReader(file))
	response := pb.EchoResponse{}
	response.Message = request.Message
	// Once the UDF is done executing, it should write the response (EchoResponse
	// in this case) to the provided fd.
	WriteResponseToFd(file, response)

}
