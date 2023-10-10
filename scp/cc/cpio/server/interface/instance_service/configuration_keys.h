/*
 * Copyright 2022 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef CPIO_SERVER_INTERFACE_INSTANCE_SERVICE_CONFIGURATION_KEYS_H_
#define CPIO_SERVER_INTERFACE_INSTANCE_SERVICE_CONFIGURATION_KEYS_H_

namespace google::scp::cpio {
// Optional. If not set, use the default value 2. The number of
// CompletionQueues for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kInstanceClientCompletionQueueCount[] =
    "cmrt_sdk_instance_client_completion_queue_count";
// Optional. If not set, use the default value 2. Minimum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kInstanceClientMinPollers[] =
    "cmrt_sdk_instance_client_min_pollers";
// Optional. If not set, use the default value 5. Maximum number of polling
// threads for the server.
// https://grpc.github.io/grpc/cpp/classgrpc_1_1_server_builder.html#aff66bd93cba7d4240a64550fe1fca88d
static constexpr char kInstanceClientMaxPollers[] =
    "cmrt_sdk_instance_client_max_pollers";
// Optional. If not set, use the default value 2.
static constexpr char kInstanceClientCpuThreadCount[] =
    "cmrt_sdk_instance_client_cpu_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kInstanceClientCpuThreadPoolQueueCap[] =
    "cmrt_sdk_instance_client_cpu_thread_pool_queue_cap";
// Optional. If not set, use the default value 2.
static constexpr char kInstanceClientIoThreadCount[] =
    "cmrt_sdk_instance_client_io_thread_count";
// Optional. If not set, use the default value 100000.
static constexpr char kInstanceClientIoThreadPoolQueueCap[] =
    "cmrt_sdk_instance_client_io_thread_pool_queue_cap";
}  // namespace google::scp::cpio

#endif  // CPIO_SERVER_INTERFACE_INSTANCE_SERVICE_CONFIGURATION_KEYS_H_
