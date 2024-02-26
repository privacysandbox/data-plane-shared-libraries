#!/bin/bash
# Copyright 2023 Google LLC
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
# Library of functions for rewriting WORKSPACE files.
#

# Require this script to be sourced rather than executed.
if ! (return 0 2>/dev/null); then
  printf "Error: Script %s must be sourced\n" "${BASH_SOURCE[0]}" &>/dev/stderr
  exit 1
fi

function buildozer::replace_repo_step1() {
  declare -r REPO="$1"
  declare -r LOCAL_PATH="$2"
  declare -r REPLACED="$3"
  cat <<EOF
set name ${REPLACED}
new local_repository ${REPO} before ${REPLACED}
EOF
}

function buildozer::replace_repo_step2() {
  declare -r REPO="$1"
  declare -r LOCAL_PATH="$2"
  declare -r REPLACED="$3"
  cat <<EOF
set path ${LOCAL_PATH} ${REPO}
EOF
}

function buildozer::generated_content_warning() {
    cat <<'EOF'
# GENERATED FILE: DO NOT EDIT
# NOTE: this rewritten WORKSPACE file is for use within a build-system container only
# WARNING: do not commit this to your repo
EOF
}

#######################################
# Configure and export the WORKSPACE variable in kokoro
# Arguments:
#   REPO
#   LOCAL_PATH
#   IN_WORKSPACE_FILE
# Example Usage to write to WORKSPACE.build-system:
#   buildozer::rewrite_workspace REPO_NAME /src/REPO_NAME WORKSPACE >WORKSPACE.build-system
#######################################
function buildozer::rewrite_workspace() {
  declare -r REPO="$1"
  declare -r LOCAL_PATH="$2"
  declare -r REPLACED="REPLACED_${REPO}"
  declare -r IN_WORKSPACE_FILE="$3"
  {
    buildozer::generated_content_warning
    printf "\n\n"
    readarray -t step1 < <(buildozer::replace_repo_step1 "${REPO}" "${LOCAL_PATH}" "${REPLACED}")
    readarray -t step2 < <(buildozer::replace_repo_step2 "${REPO}" "${LOCAL_PATH}" "${REPLACED}")
    cat "${IN_WORKSPACE_FILE}" \
      | builders/tools/buildozer -stdout "${step1[@]}" -:"${REPO}" \
      | builders/tools/buildozer -stdout "${step2[@]}" -:"${REPO}"
    printf "\n\n"
    buildozer::generated_content_warning
  }
}
