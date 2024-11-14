/*
 * Copyright 2024 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


import java.io.File;
import java.io.FileDescriptor;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.reflect.Field;
import java.net.URL;


import javax.swing.JTable.PrintMode;

import com.google.protobuf.Parser;
import privacy_sandbox.roma_byob.example.SampleUdfInterface.SampleRequest;
import privacy_sandbox.roma_byob.example.SampleUdfInterface.SampleResponse;
import privacy_sandbox.roma_byob.example.SampleUdfInterface.FunctionType;
class SampleUdf {

  static final int PRIME_COUNT = 100000;

  private SampleRequest readRequestFromFile(FileDescriptor fileDescriptor) throws FileNotFoundException, IOException {
    FileInputStream stream = new FileInputStream(fileDescriptor);
    Parser<SampleRequest> parser = SampleRequest.parser();
    return parser.parseDelimitedFrom(stream);
  }

  private void writeResponseToFile(FileDescriptor fileDescriptor, SampleResponse response) throws IOException {
    FileOutputStream output = new FileOutputStream(fileDescriptor);
    response.writeDelimitedTo(output);
  }

  void primeSieve(SampleResponse.Builder builder) {
    // Create a boolean array of size n+1
    boolean[] primes = new boolean[PRIME_COUNT+ 1];
    // Set all values to true initially
    java.util.Arrays.fill(primes, true);
    // Set first two values to false
    primes[0] = false;
    primes[1] = false;
    // Loop through the elements
    for (int i = 2; i <= Math.sqrt(PRIME_COUNT); i++) {
        if (primes[i]) {
            for (int j = i * i; j <= PRIME_COUNT; j += i) {
                primes[j] = false;
            }
        }
    }
    // Loop through the array from 2 to n
    for (int i = 2; i <= PRIME_COUNT; i++) {
        if (primes[i]) {
            builder.addPrimeNumber(i);
        }
    }
  }

  public void handleRequest(FileDescriptor fileDescriptor) {
    SampleRequest binRequest = null;
    try {
        binRequest= readRequestFromFile(fileDescriptor);
    } catch (FileNotFoundException e) {
        System.err.println("File not found");
        System.exit(1);
    } catch (IOException ex) {
        System.err.println("Failed to parse request");
        System.exit(1);
    }
    if (binRequest == null) {
        return;
    }
    SampleResponse binResponse = null;
    switch (binRequest.getFunction()) {
        case FUNCTION_HELLO_WORLD:
          binResponse = SampleResponse.newBuilder().setGreeting("Hello, world from Java!").build();
          break;
        case FUNCTION_PRIME_SIEVE:
          SampleResponse.Builder builder = SampleResponse.newBuilder();
          primeSieve(builder);
          binResponse = builder.build();
          break;
        default:
            System.exit(1);
      }
      try {
        writeResponseToFile(fileDescriptor, binResponse);
      } catch (IOException ex) {
        System.err.println("Failed to write response");
        System.exit(1);
      }
  }
  public static void main(String[] args) {
    if (args.length != 1) {
      System.err.println("Expecting exactly one argument");
      System.exit(1);
    }
    int fd = Integer.parseInt(args[0]);
    if (fd < 0) {
      System.err.println("Invalid file descriptor");
      System.exit(1);
    }
    FileDescriptor fileDescriptor = null;
    try {
      fileDescriptor = new FileDescriptor();
      Field fdField = FileDescriptor.class.getDeclaredField("fd");
      fdField.setAccessible(true);
      fdField.setInt(fileDescriptor, fd);
    } catch (NoSuchFieldException | IllegalAccessException ex) {
      ex.printStackTrace();
      System.exit(1);
    }
    SampleUdf sampleUdf = new SampleUdf();
    sampleUdf.handleRequest(fileDescriptor);
  }

  }
