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
	proto "github.com/golang/protobuf/proto"
	pb "github.com/privacy-sandbox/data-plane-shared/apis/roma/binary/example"
	"log"
	"os"
)

func main() {
	request := &pb.EchoRequest{}
	buf := make([]byte, 10)
	// Any initialization work can be done before this point.
	// The following line will result in a blocking read being performed by the
	// binary i.e. waiting for input before execution.
	// The EchoRequest proto is defined by the Trusted Server team.
	// The UDF reads request from stdin.
	reader := bufio.NewReader(os.Stdin)
	n, err := reader.Read(buf)
	if err != nil {
		log.Fatal("Failed to read from stdin: ", err)
	}
	if err := proto.Unmarshal(buf[:n], request); err != nil {
		log.Fatal("Failed to parse SampleRequest: ", err)
	}

	response := &pb.EchoResponse{}
	response.Message = request.Message
	output, err := proto.Marshal(response)
	if err != nil {
		log.Fatal("Failed to serialize EchoResponse: ", err)
	}

	// Once the UDF is done executing, it should write the response (EchoResponse
	// in this case) to the stdout.
	_, err = os.Stdout.Write(output)
	if err != nil {
		log.Print("Failed to write output to pipe: ", err)
	}
}
