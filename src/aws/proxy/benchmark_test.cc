// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// To run these benchmarks:
//   builders/tools/bazel-debian run src/aws/proxy:benchmark_test \
//   --test_output=all \
//   -- \
//   --benchmark_repetitions=1 \
//   --benchmark_time_unit=ms

#include <thread>

#include <benchmark/benchmark.h>
#include <boost/asio.hpp>

#include "src/aws/proxy/config.h"
#include "src/aws/proxy/proxy_server.h"

using boost::asio::io_service;
using boost::asio::ip::tcp;
using google::scp::proxy::Config;
using google::scp::proxy::ProxyServer;

namespace {

// Benchmark how long it takes to start and stop the ProxyServer with different
// numbers of worker threads.
// * State[0] is the number of threads to start.
// * state[1] is whether to use vSock or TCP.
void BM_StartAndStopThreads(benchmark::State& state) {
  const int number_of_threads = state.range(0);
  Config config;
  config.socks5_port = 0;  // This will have the OS pick a free port for us.
  config.vsock = (state.range(1) == 1);

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    ProxyServer server(config);
    server.BindListen();
    std::thread server_thread([&number_of_threads, &server] {
      server.Run(/*concurrency=*/number_of_threads);
    });
    server.Stop();
    server_thread.join();
  }
}

void BM_TcpConnect(benchmark::State& state) {
  Config config;
  config.socks5_port = 0;  // This will have the OS pick a free port for us.
  config.vsock = false;

  ProxyServer server(config);
  server.BindListen();
  std::thread server_thread([&server] { server.Run(/*concurrency=*/1); });

  const uint16_t port = server.Port();
  boost::asio::io_service ios;

  // Each benchmark routine has exactly one `for (auto s : state)` loop, this
  // is what's timed.
  for (auto _ : state) {
    tcp::endpoint endpoint(boost::asio::ip::address::from_string("127.0.0.1"),
                           port);
    tcp::socket socket(ios);
    socket.connect(endpoint);
  }
  server.Stop();
  server_thread.join();
}

}  // namespace

BENCHMARK(BM_StartAndStopThreads)
    ->ArgsProduct({// Run with 1-16 threads, in multiples of 2:
                   benchmark::CreateRange(1, 16, /*multi=*/2),
                   // Run with either TCP (0) or vSock(1):
                   benchmark::CreateDenseRange(0, 1, /*step=*/1)});
BENCHMARK(BM_TcpConnect);

BENCHMARK_MAIN();
