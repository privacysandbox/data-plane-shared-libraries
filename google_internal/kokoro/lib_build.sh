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

#
# library of build-related functions
#

#######################################
# Configure and export the WORKSPACE variable in kokoro
# Globals:
#   WORKSPACE (optional)
#######################################
function kokoro::set_workspace() {
  if [[ -n ${KOKORO_ARTIFACTS_DIR} ]]; then
    WORKSPACE="${KOKORO_ARTIFACTS_DIR}/git/servers-common"
  elif [[ -z ${WORKSPACE} ]]; then
    WORKSPACE="$(git rev-parse --show-toplevel)"
  fi
  export WORKSPACE
}

#######################################
# Execute tool build_and_test_all_in_docker
# Globals:
#   WORKSPACE (optional)
#   KOKORO_ARTIFACTS_DIR (optional)
#######################################
function kokoro::configure_build_env() {
  if [[ -n $1 ]]; then
    declare -n _image_list=$1
  else
    declare -a -r _image_list=(
      presubmit
      build-debian
      test-tools
      utils
    )
  fi

  export BAZEL_STARTUP_ARGS="--bazelrc=google_internal/.bazelrc"
  declare -a _BAZEL_ARGS=(
    "--config=rbecache"
  )
  declare -a _BAZEL_KOKORO_ARGS
  if [[ -v KOKORO_ARTIFACTS_DIR ]] && ! [[ -v EXTERNAL_REPO_TEST ]]; then
    trap "sleep 5s" RETURN
    _BAZEL_KOKORO_ARGS+=(
      "--config=kokoro"
    )
  fi

  if ! [[ -v WORKSPACE ]]; then
    kokoro::set_workspace
  fi

  declare -r GAR_HOST=us-docker.pkg.dev
  declare -r GAR_PROJECT=kiwi-air-force-remote-build
  declare -r GAR_REPO="${GAR_HOST}/${GAR_PROJECT}/privacysandbox/builders"
  declare -r _gcloud=/usr/bin/gcloud

  if [[ -z ${KOKORO_ARTIFACTS_DIR} ]]; then
    declare -r _ACCOUNT="${USER}@google.com"
    if [[ $("${_gcloud}" config get account) != "${_ACCOUNT}" ]]; then
      printf "Error. Set default gcloud account using \`gcloud config set account %s\`\n" "${_ACCOUNT}"
      return 1
    fi
    if [[ $("${_gcloud}" config get project) != "${GAR_PROJECT}" ]]; then
      printf "Error. Set default gcloud project using \`gcloud config set project %s\`\n" "${GAR_PROJECT}"
      return 1
    fi
  fi

  # update gcloud if it doesn't support artifact registry
  if ! "${_gcloud}" artifacts --help &>/dev/null; then
    yes | "${_gcloud}" components update
  fi

  # update docker config to use gcloud for auth to required artifact registry repo
  if ! yes | "${_gcloud}" auth configure-docker ${GAR_HOST} >/dev/null; then
    printf "Error configuring docker for Artifact Registry [%s]\n" "${GAR_HOST}"
    return 1
  fi

  # test connecting to GAR repo
  if ! "${_gcloud}" artifacts docker images list "${GAR_REPO}/presubmit" --include-tags --limit 1 >/dev/null; then
    printf "Error connecting to Artifact Registry [%s]\n" "${GAR_REPO}"
    return 1
  fi

  for IMAGE in "${_image_list[@]}"; do
    printf "Pulling or generating image [%s]\n" "${IMAGE}"
    if ! "${WORKSPACE}"/google_internal/tools/pull_builder_image --image ${IMAGE}; then
      printf "Error pulling, regenerating or pushing image [%s]\n" "${IMAGE}"
      return 1
    fi
  done

  export BAZEL_DIRECT_ARGS="${_BAZEL_ARGS[*]} --google_default_credentials"
  declare -a DOCKER_RUN_ARGS
  # optionally set credentials (likely useful only if executing this outside kokoro)
  declare -r HOST_CREDS_JSON="${HOME}/.config/gcloud/application_default_credentials.json"
  if [[ -s ${HOST_CREDS_JSON} ]]; then
    declare -r CREDS_JSON=/gcloud/application_default_credentials.json
    export BAZEL_EXTRA_ARGS="${_BAZEL_ARGS[*]} --google_credentials=${CREDS_JSON}"
    DOCKER_RUN_ARGS+=(
      "--volume ${HOST_CREDS_JSON}:${CREDS_JSON}"
    )
  else
    export BAZEL_EXTRA_ARGS="${BAZEL_DIRECT_ARGS} ${_BAZEL_KOKORO_ARGS[*]}"
  fi

  export EXTRA_DOCKER_RUN_ARGS="${DOCKER_RUN_ARGS[*]}"
}

#######################################
# Extract logs within dist tree
# Arguments:
#   * path of zip archive relative to the dist dir
#   * output directory relative to the dist/logs dir (optional)
#######################################
function kokoro::extract_dist_logs() {
  declare -r LOG_ZIP="$1"
  declare -r LOGS_SUBDIR="$2"
  declare -r DIST_LOG_DIR="${WORKSPACE}"/dist/logs
  if [[ -v KOKORO_ARTIFACTS_DIR ]]; then
    if [[ -s ${WORKSPACE}/dist/${LOG_ZIP} ]]; then
      mkdir -p "${DIST_LOG_DIR}"
      EXTRA_DOCKER_RUN_ARGS="--workdir /src/workspace/dist" \
        "${WORKSPACE}"/builders/tools/unzip -o -d logs/"${LOGS_SUBDIR}" "${LOG_ZIP}"
    fi
  fi
}

#######################################
# Set up credentials and configs for accessing GoB repos from Kokoro workers.
# Globals:
#   KOKORO_ARTIFACTS_DIR (required)
#######################################
function kokoro::setup_git_auth() {
  if [[ -z ${KOKORO_ARTIFACTS_DIR} ]]; then
    printf "kokoro::setup_git_auth: KOKORO_ARTIFACTS_DIR not set\n"
    return
  fi

  git clone https://gerrit.googlesource.com/gcompute-tools \
     "${KOKORO_ARTIFACTS_DIR}"/gcompute-tools || \
     echo "Warn: gcompute-tools already exists"
  python3 "${KOKORO_ARTIFACTS_DIR}"/gcompute-tools/git-cookie-authdaemon

  # Set up URL rewrite (We don't have sso in VM).
  git config --global \
    url.https://team.googlesource.com/.insteadOf sso://team/

  # Kokoro will redirect https to rpc by default. The git auth only works
  # on https.
  declare -a -r _team_repos=(
    "kiwi-air-force-eng-team/kv-server"
    "potassium-engprod-team/functionaltest-system"
    "android-privacy-sandbox-remarketing/fledge/servers/bidding-auction-server"
    "kiwi-air-force-eng-team/build-system"
  )
  for _team_repo in "${_team_repos[@]}"; do
    git config --global url.https://team.googlesource.com/"${_team_repo}".insteadOf rpc://team/"${_team_repo}"
  done
}

function kokoro::init_perfgate() {
  printf "Initializing Perfgate uploader\n"
  local -r PERFGATE_ARCH="$("${WORKSPACE}"/builders/tools/get-architecture)"
  printf "Using architecture: [%s]\n" "${PERFGATE_ARCH}"

  # Load the benchmark_uploader app
  local -r PERFGATE_APP_PATH=chrome/privacy_sandbox/kiwi_air_force/perfgate
  local -r PERFGATE_IMAGE=benchmark_uploader_main_gce_"${PERFGATE_ARCH}"_image
  local -r PERFGATE_TAR_DIR="${KOKORO_BLAZE_DIR}/perfgate_uploader_tar/blaze-bin/${PERFGATE_APP_PATH}"
  local -r PERFGATE_TAR_PATH="${PERFGATE_TAR_DIR}/${PERFGATE_IMAGE}.tar"

  if ! [[ -f ${PERFGATE_TAR_PATH} ]]; then
    printf "Perfgate tar not found"
    return 1
  fi

  if ! docker load --input "${PERFGATE_TAR_PATH}"; then
    printf "Error loading perfgate tar into Docker"
    return 1
  fi

  if ! docker run --rm bazel/"${PERFGATE_APP_PATH}":"${PERFGATE_IMAGE}" --version; then
    printf "Error running perfgate inside Docker"
    return 1
  fi
}

# Upload a JSON microbenchmark file to Perfgate.
#
# Arguments:
#
# * string path to JSON file to upload
# * string name of the Perfgate benchmark to update
function kokoro::upload_to_perfgate(){
  printf "Uploading to Perfgate\n"

  local -r JSON_PATH=$1
  if ! [[ -f ${JSON_PATH} ]]; then
    printf "JSON path to upload not found"
    return 1
  fi

  local -r BENCHMARK_NAME=$2

  local -r PERFGATE_ARCH="$("${WORKSPACE}"/builders/tools/get-architecture)"
  printf "Using architecture: [%s]\n" "${PERFGATE_ARCH}"

  # Load the benchmark_uploader app
  local -r PERFGATE_APP_PATH=chrome/privacy_sandbox/kiwi_air_force/perfgate
  local -r PERFGATE_IMAGE=benchmark_uploader_main_gce_"${PERFGATE_ARCH}"_image
  local -r PERFGATE_TAR_DIR="${KOKORO_BLAZE_DIR}/perfgate_uploader_tar/blaze-bin/${PERFGATE_APP_PATH}"
  local -r PERFGATE_TAR_PATH="${PERFGATE_TAR_DIR}/${PERFGATE_IMAGE}.tar"

  if ! [[ -f ${PERFGATE_TAR_PATH} ]]; then
    printf "Perfgate tar not found"
    return 1
  fi

  if ! docker load --input "${PERFGATE_TAR_PATH}"; then
    printf "Error loading perfgate tar into Docker"
    return 1
  fi

  # Flags to mount input files
  local -a -r DOCKER_FLAGS=(
      --rm
      --volume "${JSON_PATH}":/data/benchmark.json
    )

  # The --uid= flag is added because of:
  # https://g3doc.corp.google.com/testing/performance/perfgate/g3doc/guide/gce.md?cl=head#runtime-environment
  if ! docker run "${DOCKER_FLAGS[@]}" \
      bazel/"${PERFGATE_APP_PATH}":"${PERFGATE_IMAGE}" \
      --file_pattern=/data/benchmark.json \
      --benchmark="${BENCHMARK_NAME}" \
      --uid= \
      --alsologtostderr \
      ; then
    printf "Error running perfgate inside Docker"
    return 1
  fi
}
