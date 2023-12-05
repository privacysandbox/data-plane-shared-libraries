#!/bin/bash
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

# This file, simulates what an application would do inside enclave. Here we use
# curl to access www.google.com, though preloaded library and proxy.

set -o errexit
set -o nounset
set -o pipefail

readonly proxy_path="$1"
readonly preload_path="$2"
readonly proxify_path="$3"

if [[ ! -x ${proxy_path} ]]; then
  printf "Proxy: %s is not accessible\n" "${proxy_path}"
  exit 1
fi

if [[ ! -f ${preload_path} ]]; then
  printf "Preload lib: %s is not accessible" "${preload_path}"
  exit 1
fi

declare -r -i -x PROXY_PARENT_PORT=8888
declare -r -x RES_OPTIONS="use-vc"
# Use loopback address 1 as proxy address
declare -r -i -x PROXY_PARENT_CID=1

# Run the proxy, get pid.
"${proxy_path}" --port ${PROXY_PARENT_PORT} &
declare -i -r proxy_pid=$!
sleep 1


# TODO: change this to not depend on google.com
google_com_len1=$(LD_PRELOAD="${preload_path}" curl -s https://www.google.com | wc -c)
readonly google_com_len1
if [[ ${google_com_len1} -lt 4096 ]]; then
  printf "Preload lib: google.com returned less than 4KiB\n"
  kill ${proxy_pid}
  exit 1
fi

# Test proxify
google_com_len2=$("${proxify_path}" -- curl -s https://www.google.com | wc -c)
readonly google_com_len2
kill ${proxy_pid}
if [[ ${google_com_len2} -lt 4096 ]]; then
  printf "Proxify: google.com returned less than 4KiB\n"
  exit 1
fi
