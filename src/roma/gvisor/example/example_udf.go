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
	"encoding/binary"
	proto "github.com/golang/protobuf/proto"
	ptypes "github.com/golang/protobuf/ptypes/any"
	pb "github.com/privacy-sandbox/data-plane-shared/apis/roma/binary/example"
	"log"
	"os"
	"strconv"
)

func main() {
	if len(os.Args) < 2 {
		log.Fatal("Not enough input arguments")
	}
	fd, err := strconv.Atoi(os.Args[1])
	if err != nil {
		log.Fatal("Missing file descriptor: ", err)
	}
	request := &pb.EchoRequest{}
	file := os.NewFile(uintptr(fd), "socket")
	defer file.Close()
	// Any initialization work can be done before this point.
	// The following line will result in a blocking read being performed by the
	// binary i.e. waiting for input before execution.
	// The EchoRequest proto is defined by the Trusted Server team.
	// The UDF reads request from the provided fd.
	reader := bufio.NewReader(file)
	n, err := binary.ReadUvarint(reader)
	if err != nil {
		log.Fatal("Failed to read varint from stdin: ", err)
	}
	buf := make([]byte, n)
	_, err = reader.Read(buf)
	if err != nil {
		log.Fatal("Failed to read proto from stdin: ", err)
	}
	{
		var any ptypes.Any
		if err := proto.Unmarshal(buf, &any); err != nil {
			log.Fatal("Failed to unmarshal any: ", err)
		}
		if err := any.UnmarshalTo(request); err != nil {
			log.Fatal("Failed to unmarshal input: ", err)
		}
	}
	response := &pb.EchoResponse{}
	response.Message = request.Message
	// Once the UDF is done executing, it should write the response (EchoResponse
	// in this case) to the provided fd.
	var any ptypes.Any
	if err := any.MarshalFrom(response); err != nil {
		log.Fatal("Failed to marshal output: ", err)
	}
	buffer := proto.NewBuffer([]byte{})
	if err := buffer.EncodeMessage(&any); err != nil {
		log.Fatal("Failed to encode output: ", err)
	}
	if _, err := file.Write(buffer.Bytes()); err != nil {
		log.Fatal("Failed to write output: ", err)
	}
}
