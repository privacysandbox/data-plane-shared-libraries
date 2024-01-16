# Copyright 2022 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:defs.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])  # Apache 2.0

exports_files(["LICENSE"])

cc_library(
    name = "aws_c_mqtt",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-mqtt/source/*.c",
        "crt/aws-crt-cpp/crt/aws-c-mqtt/source/v5/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-mqtt/include/aws/mqtt/*.h",
        "crt/aws-crt-cpp/crt/aws-c-mqtt/include/aws/mqtt/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-mqtt/include/aws/mqtt/private/v5/*.h",
        "crt/aws-crt-cpp/crt/aws-c-mqtt/include/aws/mqtt/v5/*.h",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-mqtt/include",
    ],
    deps = [
        "aws_c_cal",
        ":aws_c_common",
        ":aws_c_compression",
        ":aws_c_http",
        ":aws_c_io",
    ],
)

cc_library(
    name = "aws_c_s3",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-s3/source/*.c",
        "crt/aws-crt-cpp/crt/aws-c-s3/source/s3_endpoint_resolver/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-s3/include/aws/s3/*.h",
        "crt/aws-crt-cpp/crt/aws-c-s3/include/aws/s3/private/*.h",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-s3/include",
    ],
    deps = [
        ":aws_c_auth",
        ":aws_c_cal",
        ":aws_c_common",
        ":aws_c_compression",
        ":aws_c_http",
        ":aws_c_io",
        ":aws_c_sdkutils",
        ":aws_checksums",
    ],
)

cc_library(
    name = "aws_checksums",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-checksums/source/*.c",
        "crt/aws-crt-cpp/crt/aws-checksums/source/generic/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-checksums/include/aws/checksums/*.h",
        "crt/aws-crt-cpp/crt/aws-checksums/include/aws/checksums/private/*.h",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-checksums/include",
    ],
    deps = [
        ":aws_c_common",
    ],
)

cc_library(
    name = "aws_c_event_stream",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-event-stream/source/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-event-stream/include/aws/event-stream/*.h",
        "crt/aws-crt-cpp/crt/aws-c-event-stream/include/aws/event-stream/private/*.h",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-event-stream/include",
    ],
    deps = [
        ":aws_c_cal",
        ":aws_c_common",
        ":aws_c_io",
        ":aws_checksums",
    ],
)

cc_library(
    name = "aws_c_sdkutils",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-sdkutils/source/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-sdkutils/include/aws/sdkutils/*.h",
        "crt/aws-crt-cpp/crt/aws-c-sdkutils/include/aws/sdkutils/private/*.h",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-sdkutils/include",
    ],
    deps = [
        ":aws_c_common",
    ],
)

cc_library(
    name = "aws_c_auth",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-auth/include/aws/auth/*.h",
        "crt/aws-crt-cpp/crt/aws-c-auth/include/aws/auth/external/*.h",
        "crt/aws-crt-cpp/crt/aws-c-auth/include/aws/auth/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-auth/source/*.c",
        "crt/aws-crt-cpp/crt/aws-c-auth/source/external/*.c",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-auth/include",
    ],
    deps = [
        ":aws_c_compression",
        ":aws_c_http",
        ":aws_c_io",
        ":aws_c_sdkutils",
    ],
)

cc_library(
    name = "s2n_tls",
    srcs = glob([
        "crt/aws-crt-cpp/crt/s2n/crypto/*.c",
        "crt/aws-crt-cpp/crt/s2n/crypto/*.h",
        "crt/aws-crt-cpp/crt/s2n/error/*.c",
        "crt/aws-crt-cpp/crt/s2n/error/*.h",
        "crt/aws-crt-cpp/crt/s2n/pq-crypto/*.c",
        "crt/aws-crt-cpp/crt/s2n/pq-crypto/*.h",
        "crt/aws-crt-cpp/crt/s2n/pq-crypto/kyber_r3/*.c",
        "crt/aws-crt-cpp/crt/s2n/pq-crypto/kyber_r3/*.h",
        "crt/aws-crt-cpp/crt/s2n/stuffer/*.c",
        "crt/aws-crt-cpp/crt/s2n/stuffer/*.h",
        "crt/aws-crt-cpp/crt/s2n/tls/*.c",
        "crt/aws-crt-cpp/crt/s2n/tls/*.h",
        "crt/aws-crt-cpp/crt/s2n/tls/extensions/*.c",
        "crt/aws-crt-cpp/crt/s2n/tls/extensions/*.h",
        "crt/aws-crt-cpp/crt/s2n/utils/*.c",
        "crt/aws-crt-cpp/crt/s2n/utils/*.h",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/s2n/api/unstable/*.h",
    ]) + ["crt/aws-crt-cpp/crt/s2n/api/s2n.h"],
    copts = [
        "-iquote $(GENDIR)/crt/aws-crt-cpp/crt/s2n/",
        "-isystem $(GENDIR)/crt/aws-crt-cpp/crt/s2n/",
        "-isystem external/aws_sdk_cpp/crt/aws-crt-cpp/crt/s2n/",
    ],
    defines = [
        "BUILD_S2N=true",
        "BUILD_SHARED_LIBS=ON",
        "BUILD_TESTING=0",
        "DISABLE_WERROR=ON",
        "S2N_LIBCRYPTO=boringssl",
    ],
    includes = [
        "crt/aws-crt-cpp/crt/s2n/api",
    ],
    deps = [
        "@boringssl//:crypto",
        "@boringssl//:ssl",
    ],
)

cc_library(
    name = "aws_c_cal",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-cal/include/aws/cal/*.h",
        "crt/aws-crt-cpp/crt/aws-c-cal/include/aws/cal/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-cal/source/*.c",
        "crt/aws-crt-cpp/crt/aws-c-cal/source/unix/*.c",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-cal/include",
    ],
    deps = [
        ":aws_c_common",
        "@boringssl//:crypto",
        "@boringssl//:ssl",
    ],
)

cc_library(
    name = "aws_c_common",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-common/include/aws/common/*.h",
        "crt/aws-crt-cpp/crt/aws-c-common/include/aws/common/external/*.h",
        "crt/aws-crt-cpp/crt/aws-c-common/include/aws/common/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-common/source/*.c",
        "crt/aws-crt-cpp/crt/aws-c-common/source/arch/generic/*.c",
        "crt/aws-crt-cpp/crt/aws-c-common/source/external/*.c",
        "crt/aws-crt-cpp/crt/aws-c-common/source/linux/*.c",
        "crt/aws-crt-cpp/crt/aws-c-common/source/posix/*.c",
    ]),
    hdrs = [
        "crt/aws-crt-cpp/crt/aws-c-common/include/aws/common/config.h",
    ],
    defines = [
        "AWS_AFFINITY_METHOD",
        "INTEL_NO_ITTNOTIFY_API",
    ],
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-common/include",
    ],
    linkopts = ["-ldl"],
    textual_hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-common/include/**/*.inl",
    ]),
)

genrule(
    name = "common_config_h",
    srcs = [
        "crt/aws-crt-cpp/crt/aws-c-common/include/aws/common/config.h.in",
    ],
    outs = [
        "crt/aws-crt-cpp/crt/aws-c-common/include/aws/common/config.h",
    ],
    cmd = "sed 's/cmakedefine/undef/g' $< > $@",
)

cc_library(
    name = "aws_c_io",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-io/source/*.c",
        "crt/aws-crt-cpp/crt/aws-c-io/source/linux/*.c",
        "crt/aws-crt-cpp/crt/aws-c-io/source/posix/*.c",
        "crt/aws-crt-cpp/crt/aws-c-io/source/s2n/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-io/include/aws/io/*.h",
        "crt/aws-crt-cpp/crt/aws-c-io/include/aws/io/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-io/source/*.h",
        "crt/aws-crt-cpp/crt/aws-c-io/source/pkcs11/v2.40/*.h",
    ]),
    defines = [
        "USE_S2N",
        "AWS_USE_EPOLL",
    ],
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-io/include",
    ],
    deps = [
        ":aws_c_cal",
        ":aws_c_common",
        ":s2n_tls",
    ],
)

cc_library(
    name = "aws_c_http",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-http/include/aws/http/*.h",
        "crt/aws-crt-cpp/crt/aws-c-http/include/aws/http/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-http/source/*.c",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/crt/aws-c-http/include/aws/http/private/*.def",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-http/include",
    ],
    deps = [
        ":aws_c_cal",
        ":aws_c_common",
        ":aws_c_compression",
        ":aws_c_io",
    ],
)

cc_library(
    name = "aws_c_compression",
    srcs = glob([
        "crt/aws-crt-cpp/crt/aws-c-compression/include/aws/compression/*.h",
        "crt/aws-crt-cpp/crt/aws-c-compression/include/aws/compression/private/*.h",
        "crt/aws-crt-cpp/crt/aws-c-compression/source/*.c",
    ]),
    includes = [
        "crt/aws-crt-cpp/crt/aws-c-compression/include",
    ],
    deps = [
        ":aws_c_common",
    ],
)

cc_library(
    name = "crt",
    srcs = glob([
        "crt/aws-crt-cpp/source/*.cpp",
        "crt/aws-crt-cpp/source/auth/*.cpp",
        "crt/aws-crt-cpp/source/crypto/*.cpp",
        "crt/aws-crt-cpp/source/endpoints/*.cpp",
        "crt/aws-crt-cpp/source/http/*.cpp",
        "crt/aws-crt-cpp/source/io/*.cpp",
        "crt/aws-crt-cpp/source/iot/*.cpp",
        "crt/aws-crt-cpp/source/mqtt/*.cpp",
    ]),
    hdrs = glob([
        "crt/aws-crt-cpp/include/aws/crt/*.h",
        "crt/aws-crt-cpp/include/aws/crt/auth/*.h",
        "crt/aws-crt-cpp/include/aws/crt/crypto/*.h",
        "crt/aws-crt-cpp/include/aws/crt/endpoints/*.h",
        "crt/aws-crt-cpp/include/aws/crt/http/*.h",
        "crt/aws-crt-cpp/include/aws/crt/io/*.h",
        "crt/aws-crt-cpp/include/aws/crt/mqtt/*.h",
        "crt/aws-crt-cpp/include/aws/crt/mqtt/private/*.h",
        "crt/aws-crt-cpp/include/aws/iot/*.h",
    ]) + ["crt/aws-crt-cpp/include/aws/crt/Config.h"],
    defines = [
        'AWS_CRT_CPP_VERSION=\\"0.24.11\\"',
        "AWS_CRT_CPP_VERSION_MAJOR=0",
        "AWS_CRT_CPP_VERSION_MINOR=24",
        "AWS_CRT_CPP_VERSION_PATCH=11",
        'AWS_CRT_CPP_GIT_HASH=\\"dd818f608b5b3b219d525554046a1776117e3996\\"',
    ],
    includes = ["crt/aws-crt-cpp/include/"],
    deps = [
        ":aws_c_auth",
        ":aws_c_common",
        ":aws_c_event_stream",
        ":aws_c_http",
        ":aws_c_io",
        ":aws_c_mqtt",
        ":aws_c_s3",
    ],
)

genrule(
    name = "crt_config_h",
    outs = [
        "crt/aws-crt-cpp/include/aws/crt/Config.h",
    ],
    cmd_bash = "touch '$@'",
)

cc_library(
    name = "core",
    srcs = glob([
        "src/aws-cpp-sdk-core/source/*.cpp",
        "src/aws-cpp-sdk-core/source/auth/*.cpp",
        "src/aws-cpp-sdk-core/source/auth/bearer-token-provider/*.cpp",
        "src/aws-cpp-sdk-core/source/auth/signer-provider/*.cpp",
        "src/aws-cpp-sdk-core/source/auth/signer/*.cpp",
        "src/aws-cpp-sdk-core/source/client/*.cpp",
        "src/aws-cpp-sdk-core/source/config/*.cpp",
        "src/aws-cpp-sdk-core/source/config/defaults/*.cpp",
        "src/aws-cpp-sdk-core/source/endpoint/*.cpp",
        "src/aws-cpp-sdk-core/source/endpoint/internal/*.cpp",
        "src/aws-cpp-sdk-core/source/external/cjson/*.cpp",
        "src/aws-cpp-sdk-core/source/external/tinyxml2/*.cpp",
        "src/aws-cpp-sdk-core/source/http/*.cpp",
        "src/aws-cpp-sdk-core/source/http/curl/*.cpp",
        "src/aws-cpp-sdk-core/source/http/standard/*.cpp",
        "src/aws-cpp-sdk-core/source/internal/*.cpp",
        "src/aws-cpp-sdk-core/source/monitoring/*.cpp",
        "src/aws-cpp-sdk-core/source/net/linux-shared/*.cpp",  # NET_SOURCE
        "src/aws-cpp-sdk-core/source/platform/linux-shared/*.cpp",  # PLATFORM_LINUX_SHARED_SOURCE
        "src/aws-cpp-sdk-core/source/smithy/tracing/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/base64/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/component-registry/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/crypto/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/crypto/factory/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/crypto/openssl/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/event/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/json/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/logging/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/memory/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/memory/stl/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/stream/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/threading/*.cpp",
        "src/aws-cpp-sdk-core/source/utils/xml/*.cpp",
    ]),
    hdrs = [
        "src/aws-cpp-sdk-core/include/aws/core/SDKConfig.h",
    ] + glob([
        "src/aws-cpp-sdk-core/include/aws/core/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/auth/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/auth/bearer-token-provider/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/auth/signer-provider/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/auth/signer/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/client/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/config/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/config/defaults/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/endpoint/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/endpoint/internal/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/external/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/external/cjson/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/external/tinyxml2/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/http/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/http/curl/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/http/standard/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/internal/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/monitoring/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/net/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/platform/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/platform/refs/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/base64/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/component-registry/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/crypto/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/crypto/openssl/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/event/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/json/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/logging/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/memory/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/memory/stl/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/ratelimiter/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/stream/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/threading/*.h",
        "src/aws-cpp-sdk-core/include/aws/core/utils/xml/*.h",
        "src/aws-cpp-sdk-core/include/smithy/*.h",
        "src/aws-cpp-sdk-core/include/smithy/tracing/*.h",
    ]),
    defines = [
        'AWS_SDK_VERSION_STRING=\\"1.11.238\\"',
        "AWS_SDK_VERSION_MAJOR=1",
        "AWS_SDK_VERSION_MINOR=11",
        "AWS_SDK_VERSION_PATCH=238",
        "ENABLE_OPENSSL_ENCRYPTION=1",
        "ENABLE_CURL_CLIENT=1",
        "OPENSSL_IS_BORINGSSL=1",
        "PLATFORM_LINUX",
    ],
    includes = [
        "src/aws-cpp-sdk-core/include",
    ],
    linkopts = ["-ldl"],
    deps = [
        ":crt",
        "@curl",
    ],
)

cc_library(
    name = "monitoring",
    srcs = glob([
        "generated/src/aws-cpp-sdk-monitoring/source/*.cpp",
        "generated/src/aws-cpp-sdk-monitoring/source/model/*.cpp",
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-monitoring/include/aws/monitoring/*.h",
        "generated/src/aws-cpp-sdk-monitoring/include/aws/monitoring/model/*.h",
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-monitoring/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "ec2",
    srcs = glob([
        "generated/src/aws-cpp-sdk-ec2/source/*.cpp",
        "generated/src/aws-cpp-sdk-ec2/source/model/*.cpp",
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-ec2/include/aws/ec2/*.h",
        "generated/src/aws-cpp-sdk-ec2/include/aws/ec2/model/*.h",
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-ec2/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "sts",
    srcs = glob([
        "generated/src/aws-cpp-sdk-sts/source/*.cpp",
        "generated/src/aws-cpp-sdk-sts/source/model/*.cpp",
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-sts/include/aws/sts/*.h",
        "generated/src/aws-cpp-sdk-sts/include/aws/sts/model/*.h",
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-sts/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "ssm",
    srcs = glob([
        "generated/src/aws-cpp-sdk-ssm/source/*.cpp",
        "generated/src/aws-cpp-sdk-ssm/source/model/*.cpp",
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-ssm/include/aws/ssm/*.h",
        "generated/src/aws-cpp-sdk-ssm/include/aws/ssm/model/*.h",
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-ssm/include/",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "s3",
    srcs = glob([
        "generated/src/aws-cpp-sdk-s3/source/*.cpp",  # AWS_S3_SOURCE
        "generated/src/aws-cpp-sdk-s3/source/model/*.cpp",  # AWS_S3_MODEL_SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-s3/include/aws/s3/*.h",  # AWS_S3_HEADERS
        "generated/src/aws-cpp-sdk-s3/include/aws/s3/model/*.h",  # AWS_S3_MODEL_HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-s3/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "dynamodb",
    srcs = glob([
        "generated/src/aws-cpp-sdk-dynamodb/source/*.cpp",  # SOURCE
        "generated/src/aws-cpp-sdk-dynamodb/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-dynamodb/include/aws/dynamodb/*.h",  # HEADERS
        "generated/src/aws-cpp-sdk-dynamodb/include/aws/dynamodb/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-dynamodb/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "dynamodbstreams",
    srcs = glob([
        "generated/src/aws-cpp-sdk-dynamodbstreams/source/*.cpp",  # SOURCE
        "generated/src/aws-cpp-sdk-dynamodbstreams/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-dynamodbstreams/include/aws/dynamodbstreams/*.h",  # HEADERS
        "generated/src/aws-cpp-sdk-dynamodbstreams/include/aws/dynamodbstreams/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-dynamodbstreams/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "kms",
    srcs = glob([
        "generated/src/aws-cpp-sdk-kms/source/*.cpp",  # SOURCE
        "generated/src/aws-cpp-sdk-kms/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-kms/include/aws/kms/*.h",  # HEADERS
        "generated/src/aws-cpp-sdk-kms/include/aws/kms/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-kms/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "sqs",
    srcs = glob([
        "generated/src/aws-cpp-sdk-sqs/source/*.cpp",  # SOURCE
        "generated/src/aws-cpp-sdk-sqs/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-sqs/include/aws/sqs/*.h",  # HEADERS
        "generated/src/aws-cpp-sdk-sqs/include/aws/sqs/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-sqs/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "autoscaling",
    srcs = glob([
        "generated/src/aws-cpp-sdk-autoscaling/source/*.cpp",  # SOURCE
        "generated/src/aws-cpp-sdk-autoscaling/source/model/*.cpp",  # MODEL SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-autoscaling/include/aws/autoscaling/*.h",  # HEADERS
        "generated/src/aws-cpp-sdk-autoscaling/include/aws/autoscaling/model/*.h",  # MODEL HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-autoscaling/include",
    ],
    deps = [
        ":core",
    ],
)

genrule(
    name = "core_config_h",
    outs = [
        "src/aws-cpp-sdk-core/include/aws/core/SDKConfig.h",
    ],
    cmd_bash = "touch '$@'",
)

cc_library(
    name = "transfer",
    srcs = glob([
        "src/aws-cpp-sdk-transfer/source/transfer/*.cpp",  # TRANSFER_SOURCE
    ]),
    hdrs = glob([
        "src/aws-cpp-sdk-transfer/include/aws/transfer/*.h",  # TRANSFER_HEADERS
    ]),
    includes = [
        "src/aws-cpp-sdk-transfer/include/",
    ],
    deps = [
        ":core",
        ":s3",
    ],
)

cc_library(
    name = "kinesis",
    srcs = glob([
        "generated/src/aws-cpp-sdk-kinesis/source/*.cpp",  # AWS_KINESIS_SOURCE
        "generated/src/aws-cpp-sdk-kinesis/source/model/*.cpp",  # AWS_KINESIS_MODEL_SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-kinesis/include/aws/kinesis/*.h",  # AWS_KINESIS_HEADERS
        "generated/src/aws-cpp-sdk-kinesis/include/aws/kinesis/model/*.h",  # AWS_KINESIS_MODEL_HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-kinesis/include",
    ],
    deps = [
        ":core",
    ],
)

cc_library(
    name = "sns",
    srcs = glob([
        "generated/src/aws-cpp-sdk-sns/source/*.cpp",  # AWS_SNS_SOURCE
        "generated/src/aws-cpp-sdk-sns/source/model/*.cpp",  # AWS_SNS_MODEL_SOURCE
    ]),
    hdrs = glob([
        "generated/src/aws-cpp-sdk-sns/include/aws/sns/*.h",  # AWS_SNS_HEADERS
        "generated/src/aws-cpp-sdk-sns/include/aws/sns/model/*.h",  # AWS_SNS_MODEL_HEADERS
    ]),
    includes = [
        "generated/src/aws-cpp-sdk-sns/include",
    ],
    deps = [
        ":core",
    ],
)
