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

# Use fixed manifest digest to ensure reproducible.
# https://hub.docker.com/layers/library/ubuntu/jammy-20240911.1/images/sha256-3d1556a8a18cf5307b121e0a98e93f1ddf1f3f8e092f1fddfd941254785b95d7?context=explore
# ubuntu v22.04, jammy-20240911.1
ARG BASE_IMAGE=ubuntu@sha256:58b87898e82351c6cf9cf5b9f3c20257bb9e2dcf33af051e12ce532d7f94e3fe

# ignore this hadolint error as BASE_IMAGE contains an image tag
# hadolint ignore=DL3006
FROM ${BASE_IMAGE} as libprofiler-builder
ENV CC=clang \
    CXX=clang++
ADD https://github.com/gperftools/gperftools/releases/download/gperftools-2.16/gperftools-2.16.tar.gz /build/gperftools.tar.gz
ADD https://apt.llvm.org/llvm.sh /build/llvm.sh
COPY compile_libprofiler /scripts/
RUN /scripts/compile_libprofiler

FROM docker/buildx-bin:v0.10 AS buildx-bin

# ignore this hadolint error as BASE_IMAGE contains an image tag
# hadolint ignore=DL3006
FROM ${BASE_IMAGE}
ARG LOCALE=en_US.UTF-8
ARG TARGETARCH
COPY install_pprof install_apps install_golang_apps install_go.sh generate_system_bazelrc .bazelversion /scripts/
COPY get_workspace_mount /usr/local/bin
COPY gitconfig /etc
COPY --from=buildx-bin /buildx /usr/libexec/docker/cli-plugins/docker-buildx
COPY --from=libprofiler-builder /usr/lib/libprofiler.so.* /usr/lib/x86_64-linux-gnu/
RUN \
  ln -rs /usr/lib/x86_64-linux-gnu/libprofiler.so.0.* /usr/lib/x86_64-linux-gnu/libprofiler.so.0 && \
  ln -rs /usr/lib/x86_64-linux-gnu/libprofiler.so.0.* /usr/lib/x86_64-linux-gnu/libprofiler.so

ENV BUILD_ARCH="${TARGETARCH}" \
    WORKSPACE=/src/workspace \
    CC=clang \
    TZ=Etc/UTC \
    LANG=${LOCALE} \
    LANGUAGE=${LOCALE} \
    LC_ALL=${LOCALE} \
    LC_CTYPE=${LOCALE}

RUN \
  chmod 644 /etc/gitconfig && \
  /scripts/install_apps --locale ${LOCALE} && \
  /scripts/generate_system_bazelrc --user-root-name ubuntu && \
  /scripts/install_golang_apps && \
  /scripts/install_pprof && \
  rm -rf /scripts

ENV PATH="/opt/bin:${PATH}:/usr/local/go/bin" \
    PYTHON_BIN_PATH="/opt/bin/python3" \
    PYTHON_LIB_PATH="/usr/lib/python3.9"
