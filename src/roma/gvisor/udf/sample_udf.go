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
	pb "github.com/privacysandbox/data-plane-shared/apis/roma/binary/example"
	"log"
	"math"
	"os"
	"strconv"
)

const kPrimeCount = 100_000

func runHelloWorld(binResponse *pb.GetValuesResponse) {
	message := "Hello, world from Go!"
	binResponse.Greeting = []byte(message)
}

func runPrimeSieve(binResponse *pb.GetValuesResponse) {
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
	writeFd, err := strconv.Atoi(os.Args[1])
	if err != nil {
		log.Fatal("Missing write file descriptor: ", err)
	}

	binRequest := &pb.GetValuesRequest{}
	buf := make([]byte, 10)
	reader := bufio.NewReader(os.Stdin)
	n, err := reader.Read(buf)
	if err != nil {
		log.Fatal("Failed to read from stdin: ", err)
	}
	if err := proto.Unmarshal(buf[:n], binRequest); err != nil {
		log.Fatal("Failed to parse GetValuesRequest: ", err)
	}

	binResponse := &pb.GetValuesResponse{}
	switch binRequest.Function {
	case pb.FunctionType_FUNCTION_HELLO_WORLD:
		runHelloWorld(binResponse)
	case pb.FunctionType_FUNCTION_PRIME_SIEVE:
		runPrimeSieve(binResponse)
	default:
		log.Print("Unexpected input")
		return
	}

	writeFile := os.NewFile(uintptr(writeFd), "pipe")
	defer writeFile.Close()
	output, err := proto.Marshal(binResponse)
	if err != nil {
		log.Fatal("Failed to serialize GetValuesResponse: ", err)
	}
	_, err = writeFile.Write(output)
	if err != nil {
		log.Print("Failed to write output to pipe: ", err)
	}
}
