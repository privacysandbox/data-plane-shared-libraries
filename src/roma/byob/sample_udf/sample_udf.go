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
	pb "github.com/privacysandbox/data-plane-shared/apis/roma/binary/example"
	protodelim "google.golang.org/protobuf/encoding/protodelim"
	"io"
	"log"
	"math"
	"os"
	"strconv"
)

const kPrimeCount = 100_000

func readRequestFromFd(reader protodelim.Reader) pb.SampleRequest {
	request := pb.SampleRequest{}
	err := protodelim.UnmarshalFrom(reader, &request)
	if err == io.EOF {
		os.Exit(-1)
	} else if err != nil {
		log.Fatal("Failed to read proto: ", err)
	}
	return request
}

func writeResponseToFd(writer io.Writer, response pb.SampleResponse) {
	if _, err := protodelim.MarshalTo(writer, &response); err != nil {
		log.Fatal("Failed to write response proto: %v", err)
	}
}

func runHelloWorld(binResponse *pb.SampleResponse) {
	message := "Hello, world from Go!"
	binResponse.Greeting = message
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
	binRequest := readRequestFromFd(bufio.NewReader(file))

	binResponse := pb.SampleResponse{}
	switch binRequest.Function {
	case pb.FunctionType_FUNCTION_HELLO_WORLD:
		runHelloWorld(&binResponse)
	case pb.FunctionType_FUNCTION_PRIME_SIEVE:
		runPrimeSieve(&binResponse)
	default:
		return
	}
	writeResponseToFd(file, binResponse)
}
