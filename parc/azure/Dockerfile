# syntax=docker/dockerfile:1.4
# Copyright 2024 Google LLC
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

# Building this Dockerfile requires adding a --build-context flag named
# "repo-root" pointing to the root of this repository

#
# stage: clang
#
FROM golang:1.23.4-bookworm AS clang
ARG CLANG_VER=18

ADD https://apt.llvm.org/llvm.sh /tmp/llvm.sh
RUN --mount=target=/var/lib/apt/lists,type=cache,sharing=locked \
  --mount=target=/var/cache/apt,type=cache,sharing=locked \
  rm -f /etc/apt/apt.conf.d/docker-clean \
  && apt-get update \
  && apt-get --quiet install -y --no-install-recommends lsb-release software-properties-common strace \
  && chmod +x /tmp/llvm.sh \
  && /tmp/llvm.sh ${CLANG_VER} \
  && apt-get --quiet install -y --no-install-recommends \
  clang-tidy libc++-${CLANG_VER}-dev libc++abi-${CLANG_VER}-dev libclang-${CLANG_VER}-dev \
  && for p in clang lldb lld ld.lld; do update-alternatives --install /usr/bin/${p} ${p} /usr/bin/${p}-${CLANG_VER} 100; done \
  && rm -f /tmp/llvm.sh \
  && clang --version

ENV \
  CC=clang \
  CXX=clang++ \
  TZ=Etc/UTC

#
# stage: build-tools
#
FROM clang AS build-tools
ARG TARGETARCH
ARG BAZELISK_VERSION=1.25.0

ENV GOPATH=/opt/go

# install bazelisk
RUN go install github.com/bazelbuild/bazelisk@v${BAZELISK_VERSION} \
  && ln -s ${GOPATH}/bin/bazelisk /usr/local/bin/bazel

# copy libc++ libs into /tmp/linux-gnu/
RUN if [ "${TARGETARCH}" = "arm64" ]; then \
  printf "Architecture is aarch64 (arm64)\n" \
  && mkdir -p /tmp/linux-gnu/aarch64-linux-gnu \
  && cp -r --dereference \
  /usr/lib/aarch64-linux-gnu/libc++.so.1  \
  /usr/lib/aarch64-linux-gnu/libc++abi.so.1 \
  /usr/lib/aarch64-linux-gnu/libunwind.so.1 \
  /tmp/linux-gnu/aarch64-linux-gnu/ ; \
  elif [ "${TARGETARCH}" = "amd64" ]; then \
  printf "Architecture is AMD64\n" \
  && mkdir -p /tmp/linux-gnu/x86_64-linux-gnu \
  && cp -r --dereference \
  /usr/lib/x86_64-linux-gnu/libc++.so.1  \
  /usr/lib/x86_64-linux-gnu/libc++abi.so.1 \
  /usr/lib/x86_64-linux-gnu/libunwind.so.1 \
  /tmp/linux-gnu/x86_64-linux-gnu/ ; \
  else printf "Unsupported architecture\n" && exit 1 ; fi

#
# stage: bazel
#
FROM build-tools AS bazel

COPY .bazelversion tools/generate_system_bazelrc /tmp/

ENV BAZEL_ROOT=/bazel_root

# install specified bazel version
COPY tools/generate_system_bazelrc /tmp
RUN cd /tmp \
  && touch WORKSPACE \
  && bazel version \
  && /tmp/generate_system_bazelrc \
  && rm -f WORKSPACE .bazelversion

#
# stage: bazel-workspace
#
FROM bazel AS bazel-workspace
ARG BAZEL_STARTUP_ARGS
ARG BAZEL_DIRECT_ARGS
ARG GOOGLE_APPLICATION_CREDENTIALS

ENV \
  BAZEL_CXXOPTS=-stdlib=libc++ \
  BAZEL_LINKOPTS=-lc++:-lm \
  WORKSPACE=/workspace

ADD . /workspace
WORKDIR /workspace
COPY --from=repo-root apis/ /apis-workspace/
RUN mkdir -p /apis-workspace/apis \
  && mv /apis-workspace/privacysandbox /apis-workspace/apis/

RUN --mount=type=cache,target=/root/.cache/bazel \
  --mount=type=cache,target=/bazel_root/parc_azure \
  --mount=type=secret,id=gcloud_creds \
  bazel \
  ${BAZEL_STARTUP_ARGS} \
  test \
  --config=run_all_tests \
  ${BAZEL_DIRECT_ARGS} \
  -- \
  @google_privacysandbox_apis//apis/privacysandbox/apis/parc/v0/... \
  //...

RUN --mount=type=cache,target=/root/.cache/bazel \
  --mount=type=cache,target=/bazel_root/parc_azure \
  --mount=type=secret,id=gcloud_creds \
  bazel \
  ${BAZEL_STARTUP_ARGS} \
  run \
  ${BAZEL_DIRECT_ARGS} \
  -- \
  //src/server/cpp:copy_to_dist

RUN --mount=type=cache,target=/root/.cache/bazel \
  --mount=type=cache,target=/bazel_root/parc_azure \
  --mount=type=secret,id=gcloud_creds \
  bazel \
  ${BAZEL_STARTUP_ARGS} \
  run \
  ${BAZEL_DIRECT_ARGS} \
  -- \
  @google_privacysandbox_apis//apis/privacysandbox/apis/parc/v0:copy_to_dist


#
# stage: linters
#
FROM bazel-workspace AS linters
ARG BAZEL_STARTUP_ARGS
ARG BAZEL_DIRECT_ARGS
ARG GOOGLE_APPLICATION_CREDENTIALS

RUN --mount=type=cache,target=/root/.cache/bazel \
  --mount=type=cache,target=/bazel_root/parc_azure \
  --mount=type=secret,id=gcloud_creds \
  bazel \
  ${BAZEL_STARTUP_ARGS} \
  test \
  --config=run_all_tests \
  ${BAZEL_DIRECT_ARGS} \
  --config=clang-tidy \
  -- \
  //...

#
# stage: protobuf artifacts
#
FROM scratch AS protobuf-artifacts
COPY --from=bazel-workspace /workspace/dist/*_descriptor_set.pb /

#
# stage: build artifacts
#
FROM scratch AS build-artifacts
COPY --from=bazel-workspace /workspace/dist/ /

#
# stage: ghz
#
FROM ghz:latest AS ghz
COPY --from=protobuf-artifacts /*_descriptor_set.pb /server/protobuf/

#
# stage: grpcurl
#
FROM fullstorydev/grpcurl:latest AS grpcurl
COPY --from=protobuf-artifacts /*_descriptor_set.pb /server/protobuf/

#
# stage: runtime-base
#
FROM gcr.io/distroless/cc-debian12:debug-nonroot AS runtime-base
ARG VERSION="0.0.0"
LABEL org.opencontainers.image.version="${VERSION}"
LABEL org.opencontainers.image.authors="Google Privacy Sandbox"
LABEL org.opencontainers.image.source="https://github.com/privacysandbox/data-plane-shared-libraries"

COPY --from=build-tools /tmp/linux-gnu/ /usr/lib/
COPY --from=protobuf-artifacts /*_descriptor_set.pb /server/protobuf/

#
# stage: parc-azure server runtime
#
FROM runtime-base AS parc_server_runtime
LABEL org.opencontainers.image.title="Parc Azure gRPC server"
LABEL org.opencontainers.image.description="Parc server for the Microsoft Azure"
COPY --from=bazel-workspace /workspace/dist/parc/parc* /server/bin/
ENTRYPOINT ["/server/bin/parc-azure-cpp"]
