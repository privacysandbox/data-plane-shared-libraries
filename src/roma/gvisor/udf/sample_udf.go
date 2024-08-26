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
	cbpb "github.com/privacysandbox/data-plane-shared/apis/roma/binary/callback"
	pb "github.com/privacysandbox/data-plane-shared/apis/roma/binary/example"
	"log"
	"math"
	"os"
	"strconv"
)

const kPrimeCount = 100_000

func runHelloWorld(binResponse *pb.SampleResponse) {
	message := "Hello, world from Go!"
	binResponse.Greeting = []byte(message)
}

func runPrimeSieve(binResponse *pb.SampleResponse) {
	// Create a boolean slice of size kPrimeCount+1
	primes := make([]bool, kPrimeCount+1)
	for i := range primes {
		primes[i] = true
	}

	primes[0] = false
	primes[1] = false

	for i := 2; i <= int(math.Sqrt(float64(kPrimeCount))); i++ {
		if primes[i] {
			for j := i * i; j <= kPrimeCount; j += i {
				primes[j] = false
			}
		}
	}

	for i := 2; i <= kPrimeCount; i++ {
		if primes[i] {
			binResponse.PrimeNumber = append(binResponse.PrimeNumber, int32(i))
		}
	}
}

func serializeDelimitedToFile(msg proto.Message, file *os.File) error {
	buffer := proto.NewBuffer([]byte{})
	if err := buffer.EncodeMessage(msg); err != nil {
		return err
	}
	_, err := file.Write(buffer.Bytes())
	return err
}

func parseDelimitedFromFile(msg proto.Message, file *os.File) error {
	reader := bufio.NewReader(file)
	length, err := binary.ReadUvarint(reader)
	if err != nil {
		return err
	}
	data := make([]byte, length)
	if _, err = reader.Read(data); err != nil {
		return err
	}
	return proto.Unmarshal(data, msg)
}

func runEchoCallback(file *os.File) {
	callback := &cbpb.Callback{FunctionName: "example"}
	var any ptypes.Any
	if err := any.MarshalFrom(callback); err != nil {
		log.Fatal("Failed to marshal callback: ", err)
	}
	if err := serializeDelimitedToFile(&any, file); err != nil {
		log.Fatal("Failed to write callback request: ", err)
	}
	if err := parseDelimitedFromFile(callback, file); err != nil {
		log.Fatal("Failed to read callback response: ", err)
	}
}

func main() {
	if len(os.Args) < 2 {
		log.Fatal("Not enough input arguments")
	}
	fd, err := strconv.Atoi(os.Args[1])
	if err != nil {
		log.Fatal("Missing file descriptor: ", err)
	}
	file := os.NewFile(uintptr(fd), "socket")
	defer file.Close()
	binRequest := &pb.SampleRequest{}
	{
		var any ptypes.Any
		if err := parseDelimitedFromFile(&any, file); err != nil {
			os.Exit(-1)
		}
		if err := any.UnmarshalTo(binRequest); err != nil {
			log.Fatal("Failed to unmarshal input: ", err)
		}
	}

	binResponse := &pb.SampleResponse{}
	switch binRequest.Function {
	case pb.FunctionType_FUNCTION_HELLO_WORLD:
		runHelloWorld(binResponse)
	case pb.FunctionType_FUNCTION_PRIME_SIEVE:
		runPrimeSieve(binResponse)
	case pb.FunctionType_FUNCTION_CALLBACK:
		runEchoCallback(file)
	case pb.FunctionType_FUNCTION_TEN_CALLBACK_INVOCATIONS:
		for i := 0; i < 10; i++ {
			runEchoCallback(file)
		}
	default:
		log.Print("Unexpected input")
		return
	}
	var any ptypes.Any
	if err := any.MarshalFrom(binResponse); err != nil {
		log.Fatal("Failed to marshal output: ", err)
	}
	if err := serializeDelimitedToFile(&any, file); err != nil {
		log.Fatal("Failed to write output: ", err)
	}
}
